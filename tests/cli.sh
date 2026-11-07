#!/usr/bin/env bash
# Vérifications du contrat de lancement, sans compte ni état persistant.
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
checks=0

expect() {
    local expected_status=$1 expected_text=$2 label=$3
    shift 3
    local output status
    if output=$("$project_dir/rvsh" "$@" 2>&1); then
        status=0
    else
        status=$?
    fi
    if [[ "$status" -ne "$expected_status" || "$output" != *"$expected_text"* ]]; then
        printf 'ÉCHEC : %s (code attendu %s, reçu %s)\n%s\n' "$label" "$expected_status" "$status" "$output" >&2
        exit 1
    fi
    checks=$((checks + 1))
    printf 'OK : %s\n' "$label"
}

expect 0 'rvsh -connect' 'aide longue' --help
expect 0 'rvsh -admin' 'aide courte' -h
expect 2 'mode manquant' 'mode obligatoire'
expect 2 'mode inconnu' 'ancien mode refusé' -list localhost 9000
expect 2 'mode inconnu' 'mode arbitraire refusé' commande-inconnue
expect 2 'ne prend aucun argument' 'arguments superflus aide' --help extra
expect 2 'ne prend aucun argument' 'arguments superflus admin' -admin extra
expect 2 'attend un nom' 'machine seule refusée' -connect hostroot
expect 2 'attend un nom' 'machine vide refusée' -connect '' alice
expect 2 'attend un nom' 'utilisateur vide refusé' -connect hostroot ''
expect 2 'attend un nom' 'arguments superflus connect' -connect hostroot alice extra
expect 3 'mode connect non implémenté' 'connect annonce son état réel' -connect hostroot alice
expect 3 'mode admin non implémenté' 'admin annonce son état réel' -admin
printf '%s vérifications réussies.\n' "$checks"
