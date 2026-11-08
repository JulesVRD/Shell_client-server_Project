#!/usr/bin/env python3
"""Tests fonctionnels par pseudo-terminaux, avec données et comptes jetables."""
import errno
import os
from pathlib import Path
import pty
import select
import signal
import stat
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
# Identifiants synthétiques propres aux tests ; aucun compte réel n'est utilisé.
ADMIN_PASSWORD = 'rvsh-test-admin'
USER_PASSWORD = 'rvsh-test-user'
NEW_PASSWORD = 'rvsh-test-new'


class Terminal:
    def __init__(self, env, *args):
        self.master, slave = pty.openpty()
        self.process = subprocess.Popen(
            [str(ROOT / 'rvsh'), *args], stdin=slave, stdout=slave,
            stderr=slave, env=env, cwd='/', start_new_session=True,
        )
        os.close(slave)
        self.buffer = ''

    def expect(self, text, timeout=8):
        deadline = time.monotonic() + timeout
        while text not in self.buffer:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AssertionError(f'Sortie attendue absente : {text!r}; sortie : {self.buffer!r}')
            ready, _, _ = select.select([self.master], [], [], min(remaining, .2))
            if ready:
                try:
                    data = os.read(self.master, 65536)
                except OSError as exc:
                    if exc.errno != errno.EIO:
                        raise
                    data = b''
                if not data:
                    raise AssertionError(f'Terminal fermé avant {text!r}; sortie : {self.buffer!r}')
                self.buffer += data.decode(errors='replace').replace('\r', '')
        end = self.buffer.index(text) + len(text)
        result, self.buffer = self.buffer[:end], self.buffer[end:]
        return result

    def send(self, text):
        os.write(self.master, (text + '\n').encode())

    def password(self, prompt, password):
        self.expect(prompt)
        # Laisser read -s configurer le terminal après l'affichage du prompt.
        time.sleep(.03)
        self.send(password)

    def command(self, text, prompt='root@hostroot > '):
        self.send(text)
        return self.expect(prompt)

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=4)
            except subprocess.TimeoutExpired:
                os.killpg(self.process.pid, signal.SIGKILL)
                self.process.wait(timeout=4)
        os.close(self.master)


class NetworkTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='rvsh-tests-')
        self.data = Path(self.temp.name) / 'data'
        self.env = dict(os.environ, RVSH_DATA_DIR=str(self.data))
        self.terminals = []

    def tearDown(self):
        for terminal in reversed(self.terminals):
            terminal.close()
        self.temp.cleanup()

    def terminal(self, *args):
        term = Terminal(self.env, *args)
        self.terminals.append(term)
        return term

    def admin(self, initial=True):
        term = self.terminal('-admin')
        if initial:
            term.password('Nouveau mot de passe : ', ADMIN_PASSWORD)
            term.password('Confirmation : ', ADMIN_PASSWORD)
        else:
            term.password('Mot de passe : ', ADMIN_PASSWORD)
        term.expect('root@hostroot > ')
        return term

    def add_user(self, admin, name, hosts=('machineA',)):
        admin.send(f'user add {name}')
        admin.password('Nouveau mot de passe : ', USER_PASSWORD)
        admin.password('Confirmation : ', USER_PASSWORD)
        admin.expect('root@hostroot > ')
        for host in hosts:
            admin.command(f'user grant {name} {host}')

    def connect(self, user='alice', host='machineA', password=USER_PASSWORD):
        term = self.terminal('-connect', host, user)
        term.password('Mot de passe : ', password)
        term.login_output = term.expect(f'{user}@{host} > ')
        self.assertNotIn(password, term.login_output)
        return term

    def test_initialisation_authentification_et_permissions(self):
        admin = self.admin()
        self.assertIn('hostroot', admin.command('rhost'))
        self.assertEqual(stat.S_IMODE(self.data.stat().st_mode), 0o700)
        password_file = self.data / 'users/root/password'
        self.assertEqual(stat.S_IMODE(password_file.stat().st_mode), 0o600)
        stored = password_file.read_text()
        self.assertTrue(stored.startswith('$6$'))
        self.assertNotIn(ADMIN_PASSWORD, stored)
        admin.send('exit')
        self.assertEqual(admin.process.wait(timeout=5), 0)
        self.assertEqual(list((self.data / 'sessions').iterdir()), [])
        bad = self.terminal('-admin')
        bad.password('Mot de passe : ', 'incorrect-test')
        bad.expect('authentification refusée')
        self.assertEqual(bad.process.wait(timeout=5), 1)
        good = self.admin(initial=False)
        self.assertIn('root', good.command('who'))

    def test_connexions_imbriquees_su_et_mot_de_passe_global(self):
        admin = self.admin()
        for host in ('machineA', 'machineB', 'machineC'):
            admin.command(f'host add {host}')
        self.add_user(admin, 'alice', ('machineA', 'machineB', 'machineC'))
        self.add_user(admin, 'bob', ('machineA',))
        admin.command('afinger alice Alice Exemple — binôme LO14')
        user = self.connect()
        self.assertIn('Alice Exemple', user.command('finger', 'alice@machineA > '))
        self.assertIn('réservée', user.command('host add interdit', 'alice@machineA > '))
        user.command('rconnect machineB', 'alice@machineB > ')
        user.command('rconnect machineC', 'alice@machineC > ')
        listing = user.command('rusers', 'alice@machineC > ')
        for host in ('machineA', 'machineB', 'machineC'):
            self.assertIn(f'alice\t{host}\t', listing)
        user.command('exit', 'alice@machineB > ')
        user.command('exit', 'alice@machineA > ')
        user.send('su bob')
        user.password('Mot de passe : ', USER_PASSWORD)
        user.expect('bob@machineA > ')
        self.assertIn('bob\tmachineA\t', user.command('who', 'bob@machineA > '))
        user.send('passwd')
        user.password('Mot de passe actuel : ', USER_PASSWORD)
        user.password('Nouveau mot de passe : ', NEW_PASSWORD)
        user.password('Confirmation : ', NEW_PASSWORD)
        user.expect('bob@machineA > ')
        user.send('exit')
        self.assertEqual(user.process.wait(timeout=5), 0)
        self.connect('bob', password=NEW_PASSWORD)
        old = self.terminal('-connect', 'machineA', 'bob')
        old.password('Mot de passe : ', USER_PASSWORD)
        old.expect('authentification refusée')
        self.assertEqual(old.process.wait(timeout=5), 1)
        admin.send('user passwd bob')
        admin.password('Nouveau mot de passe : ', ADMIN_PASSWORD)
        admin.password('Confirmation : ', ADMIN_PASSWORD)
        admin.expect('root@hostroot > ')
        self.connect('bob', password=ADMIN_PASSWORD)
        self.assertIn('alice', admin.command('user list'))
        self.assertIn('machineA', admin.command('host list'))

    def test_sessions_multiples_messages_et_livraison_differee(self):
        admin = self.admin()
        admin.command('host add machineA')
        self.add_user(admin, 'alice')
        self.add_user(admin, 'bob')
        one = self.connect()
        two = self.connect()
        listing = admin.command('rusers')
        self.assertEqual(listing.count('alice\tmachineA\t'), 2)
        admin.command('write alice@machineA Bonjour avec plusieurs mots')
        for term in (one, two):
            term.expect('root@hostroot : Bonjour avec plusieurs mots')
        admin.command('wall Annonce immédiate')
        for term in (one, two):
            term.expect('[wall] : Annonce immédiate')
        admin.command('wall -n Message pour la prochaine connexion')
        for term in (one, two):
            term.expect('[wall] : Message pour la prochaine connexion')
        bob = self.connect('bob')
        self.assertIn('[wall -n] : Message pour la prochaine connexion', bob.login_output)
        self.assertEqual((self.data / 'mail/bob').read_text(), '')
        self.assertEqual(len(list((self.data / 'sessions').iterdir())), 4)
        bob.send('exit')
        self.assertEqual(bob.process.wait(timeout=5), 0)
        again = self.connect('bob')
        self.assertNotIn('[wall -n]', again.login_output)
        output = again.command('who', 'bob@machineA > ')
        self.assertNotIn('[wall -n]', output)
        one.process.terminate()
        self.assertEqual(one.process.wait(timeout=5), 143)
        listing = admin.command('rusers')
        self.assertEqual(listing.count('alice\tmachineA\t'), 1)

    def test_refus_droits_noms_et_suppression_en_cours(self):
        admin = self.admin()
        admin.command('host add machineA')
        admin.command('host add machineB')
        self.add_user(admin, 'alice')
        refused = self.terminal('-connect', 'machineB', 'alice')
        refused.expect('droit d’accès invalide')
        self.assertEqual(refused.process.wait(timeout=5), 1)
        user = self.connect()
        self.assertIn('droit d’accès invalide', user.command('rconnect machineB', 'alice@machineA > '))
        self.assertIn('session', admin.command('host del machineA'))
        self.assertIn('connecté', admin.command('user del alice'))
        self.assertIn('session active', admin.command('user revoke alice machineA'))
        self.assertIn('ne peut pas', admin.command('host del hostroot'))
        self.assertIn('réservé', admin.command('user del root'))
        self.assertIn('invalide', admin.command('host add ../evasion'))
        self.assertIn('inconnue', user.command('touch /tmp/rvsh-interdit', 'alice@machineA > '))
        marker = Path(self.temp.name) / 'injection'
        self.assertIn('invalide', admin.command(f'host add $(touch${{IFS}}{marker})'))
        self.assertFalse(marker.exists())
        user.send('exit')
        self.assertEqual(user.process.wait(timeout=5), 0)
        admin.command('user revoke alice machineA')
        admin.command('user del alice')
        admin.command('host del machineA')
        self.assertFalse((self.data / 'users/alice').exists())
        self.assertFalse((self.data / 'hosts/machineA').exists())

    def test_modifications_concurrentes_et_session_tuee(self):
        admin = self.admin()
        second = self.admin(initial=False)
        # Envois simultanés dans deux processus ; aucune mise à jour ne doit se perdre.
        for index in range(8):
            admin.send(f'host add a{index}')
            second.send(f'host add b{index}')
            admin.expect('root@hostroot > ')
            second.expect('root@hostroot > ')
        hosts = admin.command('rhost')
        for index in range(8):
            self.assertIn(f'a{index}\n', hosts)
            self.assertIn(f'b{index}\n', hosts)
        self.add_user(admin, 'alice', ())
        for index in range(8):
            admin.send(f'user grant alice a{index}')
            second.send(f'user grant alice b{index}')
            admin.expect('root@hostroot > ')
            second.expect('root@hostroot > ')
        self.assertEqual(len((self.data / 'users/alice/grants').read_text().splitlines()), 16)
        for index in range(8):
            admin.send(f'user revoke alice a{index}')
            second.send(f'user revoke alice b{index}')
            admin.expect('root@hostroot > ')
            second.expect('root@hostroot > ')
        self.assertEqual((self.data / 'users/alice/grants').read_text(), '')
        admin.command('host add machineA')
        admin.command('user grant alice machineA')
        user = self.connect()
        user.process.kill()
        user.process.wait(timeout=5)
        self.assertNotIn('alice\tmachineA\t', admin.command('rusers'))
        self.assertEqual(len(list((self.data / 'sessions').iterdir())), 2)

    def test_pid_vivant_reutilise_ne_conserve_pas_une_ancienne_session(self):
        admin = self.admin()
        sessions = self.data / 'sessions'
        inbox = self.data / 'inbox'
        # Ancien format : un PID vivant ne prouve pas que la session lui appartient.
        (sessions / 'ancien-format').write_text(
            f'root\thostroot\t2000-01-01 00:00:00\t{admin.process.pid}\n')
        (sessions / 'mauvais-demarrage').write_text(
            f'root\thostroot\t2000-01-01 00:00:00\t{admin.process.pid}\tfaux:0\n')
        (inbox / 'ancien-format').write_text('message périmé\n')
        listing = admin.command('rusers')
        self.assertEqual(listing.count('root\thostroot\t'), 1)
        self.assertFalse((sessions / 'ancien-format').exists())
        self.assertFalse((sessions / 'mauvais-demarrage').exists())
        self.assertFalse((inbox / 'ancien-format').exists())
        record = next(sessions.iterdir()).read_text().strip().split('\t')
        self.assertEqual(len(record), 5)
        self.assertTrue(record[4])

    def test_etat_persistant_apres_fermeture_et_reouverture(self):
        admin = self.admin()
        admin.command('host add machineA')
        self.add_user(admin, 'alice')
        admin.command('afinger alice Profil conservé après fermeture')
        admin.command('wall -n Message conservé après fermeture')
        admin.send('exit')
        self.assertEqual(admin.process.wait(timeout=5), 0)
        self.assertEqual(list((self.data / 'sessions').iterdir()), [])
        reopened = self.admin(initial=False)
        self.assertIn('machineA', reopened.command('rhost'))
        user = self.connect()
        self.assertIn('[wall -n] : Message conservé après fermeture', user.login_output)
        self.assertIn('Profil conservé après fermeture', user.command('finger', 'alice@machineA > '))

    def test_fermeture_eof_int_et_hup_nettoie_les_sessions(self):
        admin = self.admin()
        admin.command('host add machineA')
        self.add_user(admin, 'alice')
        for sig, status in ((signal.SIGINT, 130), (signal.SIGHUP, 129)):
            user = self.connect()
            user.process.send_signal(sig)
            self.assertEqual(user.process.wait(timeout=5), status)
            self.assertNotIn('alice\tmachineA\t', admin.command('rusers'))
        user = self.connect()
        os.write(user.master, b'\x04')
        self.assertEqual(user.process.wait(timeout=5), 0)
        self.assertNotIn('alice\tmachineA\t', admin.command('rusers'))

    def test_dossier_existant_etranger_refuse_sans_modification(self):
        self.data.mkdir(mode=0o755)
        marker = self.data / 'document-a-conserver'
        marker.write_text('contenu existant')
        original_mode = stat.S_IMODE(self.data.stat().st_mode)
        term = self.terminal('-admin')
        term.expect('fichiers étrangers')
        self.assertEqual(term.process.wait(timeout=5), 1)
        self.assertEqual(marker.read_text(), 'contenu existant')
        self.assertEqual(stat.S_IMODE(self.data.stat().st_mode), original_mode)
        self.assertFalse((self.data / '.rvsh-data').exists())

    def test_initialisation_incomplete_et_mode_sans_reseau(self):
        term = self.terminal('-connect', 'machineA', 'alice')
        term.expect('réseau non initialisé')
        self.assertEqual(term.process.wait(timeout=5), 1)
        bad = self.terminal('-admin')
        bad.password('Nouveau mot de passe : ', ADMIN_PASSWORD)
        bad.password('Confirmation : ', 'autre-test')
        bad.expect('ne correspondent pas')
        self.assertEqual(bad.process.wait(timeout=5), 1)
        self.assertFalse((self.data / 'initialized').exists())
        self.admin()


if __name__ == '__main__':
    unittest.main(verbosity=2)
