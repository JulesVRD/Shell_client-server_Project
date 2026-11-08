# shellcheck shell=bash
authenticated_session_locked() {
    local user=$1 host=$2 expected=$3
    load_hash "$user" && [[ $RVSH_HASH == "$expected" ]] || {
        fail 'compte modifié ; recommencer la connexion.'; return 1;
    }
    push_session_locked "$user" "$host"
}

# Sessions imbriquées et réception des messages.

current_identity() {
    RVSH_USER=${RVSH_USERS[RVSH_DEPTH]}
    RVSH_HOST=${RVSH_HOSTS[RVSH_DEPTH]}
}

push_session_locked() {
    local user=$1 host=$2 id
    can_access "$user" "$host" || { fail "machine, utilisateur ou droit d’accès invalide."; return 1; }
    prune_sessions
    RVSH_SEQUENCE=$((RVSH_SEQUENCE + 1))
    id="$$-$RVSH_SEQUENCE"
    printf '%s\t%s\t%s\t%s\t%s\n' "$user" "$host" "$(date '+%Y-%m-%d %H:%M:%S')" "$$" "$RVSH_OWNER_ID" > "$RVSH_DATA_DIR/sessions/$id" || return 1
    RVSH_DEPTH=$((RVSH_DEPTH + 1))
    RVSH_IDS[RVSH_DEPTH]=$id
    RVSH_USERS[RVSH_DEPTH]=$user
    RVSH_HOSTS[RVSH_DEPTH]=$host
    current_identity
    deliver_offline "$user" "$id"
}

deliver_offline() {
    local user=$1 id=$2
    if [[ -s $RVSH_DATA_DIR/mail/$user ]]; then
        cat -- "$RVSH_DATA_DIR/mail/$user" >> "$RVSH_DATA_DIR/inbox/$id"
        : > "$RVSH_DATA_DIR/mail/$user"
    fi
}

pop_session_locked() {
    local id=${RVSH_IDS[RVSH_DEPTH]}
    rm -f -- "$RVSH_DATA_DIR/sessions/$id" "$RVSH_DATA_DIR/inbox/$id"
    unset 'RVSH_IDS[RVSH_DEPTH]' 'RVSH_USERS[RVSH_DEPTH]' 'RVSH_HOSTS[RVSH_DEPTH]'
    RVSH_DEPTH=$((RVSH_DEPTH - 1))
    if [[ $RVSH_DEPTH -ge 0 ]]; then current_identity; fi
}

cleanup_sessions_locked() {
    while [[ $RVSH_DEPTH -ge 0 ]]; do pop_session_locked; done
}

cleanup() {
    if [[ -n ${RVSH_NOTIFIER:-} ]]; then
        kill "$RVSH_NOTIFIER" 2>/dev/null
        wait "$RVSH_NOTIFIER" 2>/dev/null
        RVSH_NOTIFIER=''
    fi
    if [[ -n ${RVSH_DATA_DIR:-} && $RVSH_DEPTH -ge 0 ]]; then with_lock cleanup_sessions_locked; fi
}

drain_messages_locked() {
    local file user host stamp pid owner id
    for file in "$RVSH_DATA_DIR/sessions/"*; do
        [[ -f $file ]] || continue
        IFS=$'\t' read -r user host stamp pid owner < "$file"
        [[ $pid == "$$" && $owner == "$RVSH_OWNER_ID" ]] || continue
        id=${file##*/}
        if [[ -s $RVSH_DATA_DIR/inbox/$id ]]; then
            printf '\nMessages pour %s@%s :\n' "$user" "$host"
            cat -- "$RVSH_DATA_DIR/inbox/$id"
            : > "$RVSH_DATA_DIR/inbox/$id"
        fi
    done
}

start_notifier() {
    # Réouvrir le verrou : un descripteur hérité partagerait le verrou du parent.
    (
        trap - EXIT INT HUP
        trap 'exit 0' TERM
        exec 9>"$RVSH_DATA_DIR/lock" || exit 1
        while kill -0 "$$" 2>/dev/null; do
            # Ne pas attendre un verrou dans le processus de notification : il doit
            # pouvoir s'arrêter immédiatement pendant le nettoyage du parent.
            if flock -n 9; then
                drain_messages_locked
                flock -u 9
            fi
            sleep 0.2
        done
    ) &
    RVSH_NOTIFIER=$!
}

show_sessions_locked() {
    local scope=$1 file user host stamp pid owner
    prune_sessions
    for file in "$RVSH_DATA_DIR/sessions/"*; do
        [[ -f $file ]] || continue
        IFS=$'\t' read -r user host stamp pid owner < "$file"
        [[ $scope == all || $host == "$RVSH_HOST" ]] || continue
        printf '%s\t%s\t%s\n' "$user" "$host" "$stamp"
    done
}

replace_user_locked() {
    local user=$1 expected_hash=$2 id=${RVSH_IDS[RVSH_DEPTH]}
    if ! load_hash "$user" || [[ $RVSH_HASH != "$expected_hash" ]] || ! can_access "$user" "$RVSH_HOST"; then
        fail 'identité ou droits modifiés ; reconnexion refusée.'; return 1;
    fi
    printf '%s\t%s\t%s\t%s\t%s\n' "$user" "$RVSH_HOST" "$(date '+%Y-%m-%d %H:%M:%S')" "$$" "$RVSH_OWNER_ID" > "$RVSH_DATA_DIR/sessions/$id"
    RVSH_USERS[RVSH_DEPTH]=$user
    current_identity
    deliver_offline "$user" "$id"
}

run_session() {
    local line command arguments
    RVSH_OWNER_ID=$(process_identity "$$") || { fail 'métadonnées de processus Linux indisponibles dans /proc.'; return 1; }
    RVSH_DEPTH=-1 RVSH_SEQUENCE=0
    RVSH_IDS=() RVSH_USERS=() RVSH_HOSTS=()
    trap cleanup EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
    trap 'exit 129' HUP
    with_lock authenticated_session_locked "$1" "$2" "$3" || return 1
    start_notifier
    while [[ $RVSH_DEPTH -ge 0 ]]; do
        with_lock drain_messages_locked
        if [[ -t 0 ]]; then printf '%s@%s > ' "$RVSH_USER" "$RVSH_HOST"; fi
        if ! IFS= read -r line; then break; fi
        IFS=$' \t' read -r command arguments <<< "$line"
        [[ -n $command ]] || continue
        dispatch_command "$command" "$arguments"
    done
    cleanup
    trap - EXIT INT TERM HUP
    return 0
}
