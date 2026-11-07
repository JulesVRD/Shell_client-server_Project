# shellcheck shell=bash
# Stockage du réseau virtuel ; les opérations partagées passent par with_lock.

fail() { printf 'rvsh : %s\n' "$*" >&2; return 1; }
valid_name() { [[ $1 =~ ^[A-Za-z][A-Za-z0-9_-]{0,31}$ ]]; }

with_lock() {
    local status
    if [[ ${RVSH_LOCKED:-0} -eq 1 ]]; then "$@"; return $?; fi
    flock -x -w 10 9 || { fail 'stockage occupé ; réessayer.'; return 1; }
    RVSH_LOCKED=1
    "$@"
    status=$?
    RVSH_LOCKED=0
    flock -u 9
    return "$status"
}

prepare_storage() {
    command -v flock >/dev/null || { fail 'flock est requis (util-linux sous Linux).'; return 1; }
    command -v openssl >/dev/null || { fail 'OpenSSL est requis pour les mots de passe.'; return 1; }
    RVSH_DATA_DIR=${RVSH_DATA_DIR:-"$RVSH_PROJECT_DIR/data"}
    [[ ! -L $RVSH_DATA_DIR ]] || { fail 'le dossier de données ne doit pas être un lien symbolique.'; return 1; }
    umask 077
    mkdir -p -- "$RVSH_DATA_DIR" || return 1
    # Ne jamais changer les permissions d'un dossier existant sans rapport avec rvsh.
    if [[ ! -f $RVSH_DATA_DIR/.rvsh-data ]]; then
        if ! (shopt -s nullglob dotglob; entries=("$RVSH_DATA_DIR/"*); [[ ${#entries[@]} -eq 0 ]]); then
            fail 'le dossier de données existe et contient des fichiers étrangers à rvsh.'
            return 1
        fi
        : > "$RVSH_DATA_DIR/.rvsh-data" || return 1
    fi
    chmod 700 -- "$RVSH_DATA_DIR" || return 1
    RVSH_DATA_DIR=$(cd -- "$RVSH_DATA_DIR" && pwd -P) || return 1
    exec 9>"$RVSH_DATA_DIR/lock" || return 1
    with_lock create_directories
}

create_directories() {
    mkdir -p -- "$RVSH_DATA_DIR/users" "$RVSH_DATA_DIR/hosts" \
        "$RVSH_DATA_DIR/sessions" "$RVSH_DATA_DIR/inbox" "$RVSH_DATA_DIR/mail"
}

read_password() {
    local label=$1
    RVSH_PASSWORD=''
    printf '%s' "$label" >&2
    IFS= read -r -s RVSH_PASSWORD
    local status=$?
    printf '\n' >&2
    [[ $status -eq 0 ]] || { fail 'saisie interrompue.'; return 1; }
    [[ -n $RVSH_PASSWORD ]] || { fail 'mot de passe vide refusé.'; return 1; }
}

new_password_hash() {
    local first salt
    read_password 'Nouveau mot de passe : ' || return 1
    first=$RVSH_PASSWORD
    read_password 'Confirmation : ' || { unset first RVSH_PASSWORD; return 1; }
    if [[ $first != "$RVSH_PASSWORD" ]]; then
        unset first RVSH_PASSWORD
        fail 'les mots de passe ne correspondent pas.'
        return 1
    fi
    salt=$(openssl rand -hex 8) || return 1
    RVSH_NEW_HASH=$(printf '%s\n' "$first" | openssl passwd -6 -salt "$salt" -stdin)
    local status=$?
    unset first RVSH_PASSWORD
    [[ $status -eq 0 && $RVSH_NEW_HASH == \$6\$* ]] || { fail 'hachage SHA-512 crypt indisponible dans OpenSSL.'; return 1; }
}

password_matches() {
    local stored=$1 password=$2 salt computed
    [[ $stored == \$6\$* ]] || return 1
    salt=${stored#\$6\$}
    salt=${salt%%\$*}
    computed=$(printf '%s\n' "$password" | openssl passwd -6 -salt "$salt" -stdin) || return 1
    [[ $computed == "$stored" ]]
}

load_hash() {
    [[ -f $RVSH_DATA_DIR/users/$1/password ]] || return 1
    IFS= read -r RVSH_HASH < "$RVSH_DATA_DIR/users/$1/password"
}

# Le fichier temporaire reste dans le même dossier pour un renommage atomique.
write_hash() {
    local user=$1 hash=$2
    printf '%s\n' "$hash" > "$RVSH_DATA_DIR/users/$user/password.tmp" &&
        mv -- "$RVSH_DATA_DIR/users/$user/password.tmp" "$RVSH_DATA_DIR/users/$user/password"
}

bootstrap_locked() {
    [[ ! -f $RVSH_DATA_DIR/initialized ]] || return 2
    mkdir -p -- "$RVSH_DATA_DIR/users/root" "$RVSH_DATA_DIR/hosts/hostroot" || return 1
    write_hash root "$RVSH_NEW_HASH" || return 1
    : > "$RVSH_DATA_DIR/users/root/grants"
    : > "$RVSH_DATA_DIR/users/root/info"
    : > "$RVSH_DATA_DIR/initialized"
}

can_access() {
    local user=$1 host=$2
    [[ -d $RVSH_DATA_DIR/users/$user && -d $RVSH_DATA_DIR/hosts/$host ]] || return 1
    [[ $user == root ]] || grep -Fxq -- "$host" "$RVSH_DATA_DIR/users/$user/grants"
}

prune_sessions() {
    local file user host stamp pid
    for file in "$RVSH_DATA_DIR/sessions/"*; do
        [[ -f $file ]] || continue
        IFS=$'\t' read -r user host stamp pid < "$file"
        if [[ ! $pid =~ ^[0-9]+$ ]] || ! kill -0 "$pid" 2>/dev/null; then
            rm -f -- "$RVSH_DATA_DIR/inbox/${file##*/}" "$file"
        fi
    done
}

has_session() {
    local wanted_user=$1 wanted_host=$2 file user host stamp pid
    prune_sessions
    for file in "$RVSH_DATA_DIR/sessions/"*; do
        [[ -f $file ]] || continue
        IFS=$'\t' read -r user host stamp pid < "$file"
        if [[ ( -z $wanted_user || $user == "$wanted_user" ) && ( -z $wanted_host || $host == "$wanted_host" ) ]]; then return 0; fi
    done
    return 1
}
