# shellcheck shell=bash
# Commandes du shell virtuel. Aucune saisie ne passe par eval ou bash -c.

require_admin() {
    [[ $RVSH_ADMIN -eq 1 && $RVSH_USER == root ]] || { fail "commande réservée à l’administrateur."; return 1; }
}

list_hosts_locked() {
    local path
    for path in "$RVSH_DATA_DIR/hosts/"*; do [[ -d $path ]] && printf '%s\n' "${path##*/}"; done
    return 0
}

host_locked() {
    local action=$1 host=$2
    valid_name "$host" || { fail 'nom de machine invalide (lettre puis lettres, chiffres, _ ou -, 32 caractères maximum).'; return 1; }
    case "$action" in
        add)
            [[ ! -d $RVSH_DATA_DIR/hosts/$host ]] || { fail 'machine déjà existante.'; return 1; }
            mkdir -- "$RVSH_DATA_DIR/hosts/$host"
            ;;
        del)
            [[ $host != hostroot ]] || { fail 'hostroot ne peut pas être supprimée.'; return 1; }
            [[ -d $RVSH_DATA_DIR/hosts/$host ]] || { fail 'machine inconnue.'; return 1; }
            ! has_session '' "$host" || { fail 'machine utilisée par une session.'; return 1; }
            local grants
            for grants in "$RVSH_DATA_DIR/users/"*/grants; do
                [[ -f $grants ]] || continue
                awk -v host="$host" '$0 != host' "$grants" > "$grants.tmp" && mv -- "$grants.tmp" "$grants" || return 1
            done
            rmdir -- "$RVSH_DATA_DIR/hosts/$host"
            ;;
        *) fail 'usage : host add|del nom_machine ; host list' ;;
    esac
}

user_locked() {
    local action=$1 user=$2 host=${3:-} path
    valid_name "$user" || { fail "nom d’utilisateur invalide (lettre puis lettres, chiffres, _ ou -, 32 caractères maximum)."; return 1; }
    [[ $user != root ]] || { fail 'le compte root est réservé ; utiliser passwd pour son mot de passe.'; return 1; }
    case "$action" in
        add)
            [[ ! -d $RVSH_DATA_DIR/users/$user ]] || { fail 'utilisateur déjà existant.'; return 1; }
            mkdir -- "$RVSH_DATA_DIR/users/$user" || return 1
            write_hash "$user" "$RVSH_NEW_HASH" || return 1
            : > "$RVSH_DATA_DIR/users/$user/grants"
            : > "$RVSH_DATA_DIR/users/$user/info"
            ;;
        del)
            [[ -d $RVSH_DATA_DIR/users/$user ]] || { fail 'utilisateur inconnu.'; return 1; }
            ! has_session "$user" '' || { fail 'utilisateur connecté.'; return 1; }
            rm -f -- "$RVSH_DATA_DIR/users/$user/password" "$RVSH_DATA_DIR/users/$user/grants" "$RVSH_DATA_DIR/users/$user/info" "$RVSH_DATA_DIR/mail/$user"
            rmdir -- "$RVSH_DATA_DIR/users/$user"
            ;;
        grant|revoke)
            valid_name "$host" && [[ -d $RVSH_DATA_DIR/hosts/$host && -d $RVSH_DATA_DIR/users/$user ]] || { fail 'machine ou utilisateur inconnu.'; return 1; }
            path="$RVSH_DATA_DIR/users/$user/grants"
            if [[ $action == grant ]]; then
                if ! grep -Fxq -- "$host" "$path"; then printf '%s\n' "$host" >> "$path"; fi
            else
                ! has_session "$user" "$host" || { fail 'droit utilisé par une session active.'; return 1; }
                awk -v host="$host" '$0 != host' "$path" > "$path.tmp" && mv -- "$path.tmp" "$path"
            fi
            ;;
        passwd)
            [[ -d $RVSH_DATA_DIR/users/$user ]] || { fail 'utilisateur inconnu.'; return 1; }
            write_hash "$user" "$RVSH_NEW_HASH"
            ;;
        *) fail 'usage : user add|del|passwd nom ; user grant|revoke nom machine ; user list' ;;
    esac
}

list_users_locked() {
    local path grants
    for path in "$RVSH_DATA_DIR/users/"*; do
        [[ -d $path ]] || continue
        grants=$(tr '\n' ' ' < "$path/grants")
        [[ ${path##*/} != root ]] || grants='toutes les machines (administrateur)'
        printf '%s\t%s\n' "${path##*/}" "$grants"
    done
}

command_host() {
    require_admin || return 1
    if [[ $# -eq 1 && $1 == list ]]; then with_lock list_hosts_locked; return; fi
    [[ $# -eq 2 ]] || { fail 'usage : host add|del nom_machine ; host list'; return 1; }
    with_lock host_locked "$@"
}

command_user() {
    require_admin || return 1
    local status
    if [[ $# -eq 1 && $1 == list ]]; then with_lock list_users_locked; return; fi
    [[ $# -ge 2 ]] || { fail 'usage : user add|del|passwd nom ; user grant|revoke nom machine ; user list'; return 1; }
    case "$1" in
        add|passwd)
            [[ $# -eq 2 ]] && valid_name "$2" && [[ $2 != root ]] || { fail 'utilisateur ou arguments invalides.'; return 1; }
            # Ne pas immobiliser le stockage pendant une saisie de mot de passe.
            new_password_hash || return 1
            with_lock user_locked "$@"
            status=$?
            unset RVSH_NEW_HASH
            return "$status"
            ;;
        del)
            [[ $# -eq 2 ]] || { fail 'usage : user del utilisateur'; return 1; }
            with_lock user_locked "$@"
            ;;
        grant|revoke)
            [[ $# -eq 3 ]] || { fail 'usage : user grant|revoke utilisateur machine'; return 1; }
            with_lock user_locked "$@"
            ;;
        *) fail 'sous-commande user inconnue.' ;;
    esac
}

finger_locked() {
    local user=$1
    valid_name "$user" && [[ -d $RVSH_DATA_DIR/users/$user ]] || { fail 'utilisateur inconnu.'; return 1; }
    printf 'Utilisateur : %s\n' "$user"
    if [[ -s $RVSH_DATA_DIR/users/$user/info ]]; then cat -- "$RVSH_DATA_DIR/users/$user/info"; else printf 'Aucune information complémentaire.\n'; fi
}

afinger_locked() {
    local user=$1 info=$2
    valid_name "$user" && [[ -d $RVSH_DATA_DIR/users/$user ]] || { fail 'utilisateur inconnu.'; return 1; }
    printf '%s\n' "$info" > "$RVSH_DATA_DIR/users/$user/info"
}

command_su() {
    if [[ $# -ne 1 ]] || ! valid_name "$1"; then fail 'usage : su nom_utilisateur'; return 1; fi
    [[ $1 != root || $RVSH_ADMIN -eq 1 ]] || { fail 'utiliser rvsh -admin pour le compte root.'; return 1; }
    local user=$1 stored
    with_lock load_hash "$user" || { fail 'utilisateur inconnu.'; return 1; }
    stored=$RVSH_HASH
    read_password 'Mot de passe : ' || return 1
    if ! password_matches "$stored" "$RVSH_PASSWORD"; then unset RVSH_PASSWORD; fail 'authentification refusée.'; return 1; fi
    unset RVSH_PASSWORD
    with_lock replace_user_locked "$user" "$stored"
}

change_password_locked() {
    local user=$1 expected=$2
    load_hash "$user" && [[ $RVSH_HASH == "$expected" ]] || { fail 'compte modifié ; recommencer.'; return 1; }
    write_hash "$user" "$RVSH_NEW_HASH"
}

command_passwd() {
    [[ $# -eq 0 ]] || { fail 'usage : passwd'; return 1; }
    local stored status user=$RVSH_USER
    with_lock load_hash "$user" || return 1
    stored=$RVSH_HASH
    read_password 'Mot de passe actuel : ' || return 1
    if ! password_matches "$stored" "$RVSH_PASSWORD"; then unset RVSH_PASSWORD; fail 'authentification refusée.'; return 1; fi
    unset RVSH_PASSWORD
    new_password_hash || return 1
    with_lock change_password_locked "$user" "$stored"
    status=$?
    if [[ $status -eq 0 ]]; then printf 'Mot de passe modifié sur le réseau.\n'; fi
    unset RVSH_NEW_HASH
    return "$status"
}

write_message_locked() {
    local target_user=$1 target_host=$2 message=$3 file user host stamp pid owner found=0
    prune_sessions
    for file in "$RVSH_DATA_DIR/sessions/"*; do
        [[ -f $file ]] || continue
        IFS=$'\t' read -r user host stamp pid owner < "$file"
        if [[ $user == "$target_user" && $host == "$target_host" ]]; then
            printf '%s@%s : %s\n' "$RVSH_USER" "$RVSH_HOST" "$message" >> "$RVSH_DATA_DIR/inbox/${file##*/}"
            found=1
        fi
    done
    [[ $found -eq 1 ]] || { fail 'destinataire non connecté sur cette machine.'; return 1; }
}

wall_locked() {
    local offline=$1 message=$2 file user host stamp pid owner path
    prune_sessions
    # Un exemplaire par session connectée ; un exemplaire différé par compte hors ligne.
    for file in "$RVSH_DATA_DIR/sessions/"*; do
        [[ -f $file ]] || continue
        printf '%s@%s [wall] : %s\n' "$RVSH_USER" "$RVSH_HOST" "$message" >> "$RVSH_DATA_DIR/inbox/${file##*/}"
    done
    if [[ $offline -eq 1 ]]; then
        for path in "$RVSH_DATA_DIR/users/"*; do
            [[ -d $path ]] || continue
            user=${path##*/}
            if ! has_session "$user" ''; then
                printf '%s@%s [wall -n] : %s\n' "$RVSH_USER" "$RVSH_HOST" "$message" >> "$RVSH_DATA_DIR/mail/$user"
            fi
        done
    fi
}

print_commands() {
    cat <<'HELP'
who | rusers | rhost | rconnect machine | su utilisateur | passwd
finger [utilisateur] | write utilisateur@machine message | exit | help
Administration (root) :
  host add|del machine ; host list
  user add|del|passwd utilisateur ; user list
  user grant|revoke utilisateur machine
  afinger utilisateur informations
  wall message ; wall -n message
Les noms ne contiennent pas d’espaces ; les messages peuvent en contenir.
HELP
}

dispatch_command() {
    local command=$1 arguments=$2 target message user host offline=0
    local words=()
    IFS=$' \t' read -r -a words <<< "$arguments"
    case "$command" in
        help)
            [[ ${#words[@]} -eq 0 ]] || { fail 'usage : help'; return 1; }
            print_commands
            ;;
        who|rusers|rhost|exit)
            [[ ${#words[@]} -eq 0 ]] || { fail "usage : $command"; return 1; }
            case "$command" in
                who) with_lock show_sessions_locked current ;;
                rusers) with_lock show_sessions_locked all ;;
                rhost) with_lock list_hosts_locked ;;
                exit) with_lock pop_session_locked ;;
            esac
            ;;
        rconnect)
            if [[ ${#words[@]} -ne 1 ]] || ! valid_name "${words[0]}"; then fail 'usage : rconnect nom_machine'; return 1; fi
            with_lock push_session_locked "$RVSH_USER" "${words[0]}"
            ;;
        su) command_su "${words[@]}" ;;
        passwd) command_passwd "${words[@]}" ;;
        host) command_host "${words[@]}" ;;
        user) command_user "${words[@]}" ;;
        finger)
            [[ ${#words[@]} -le 1 ]] || { fail 'usage : finger [utilisateur]'; return 1; }
            with_lock finger_locked "${words[0]:-$RVSH_USER}"
            ;;
        afinger)
            require_admin || return 1
            IFS=$' \t' read -r user message <<< "$arguments"
            [[ -n $user && -n $message ]] || { fail 'usage : afinger utilisateur informations'; return 1; }
            with_lock afinger_locked "$user" "$message"
            ;;
        write)
            IFS=$' \t' read -r target message <<< "$arguments"
            [[ $target == *@* && -n $message ]] || { fail 'usage : write utilisateur@machine message'; return 1; }
            user=${target%%@*} host=${target#*@}
            if ! valid_name "$user" || ! valid_name "$host"; then fail 'destinataire invalide.'; return 1; fi
            with_lock write_message_locked "$user" "$host" "$message"
            ;;
        wall)
            require_admin || return 1
            IFS=$' \t' read -r target message <<< "$arguments"
            if [[ $target == -n ]]; then offline=1; else message=$arguments; fi
            [[ -n $message ]] || { fail 'usage : wall [-n] message'; return 1; }
            with_lock wall_locked "$offline" "$message"
            ;;
        *) fail "commande virtuelle inconnue : $command (help pour l’aide)." ;;
    esac
}
