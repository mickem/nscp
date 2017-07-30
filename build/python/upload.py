import os
import argparse

def create_token(id = None, user=None, password=None):
    from github3 import authorize
    from getpass import getuser, getpass

    if not user:
        user = getuser()
        print "Enter github username [%s]:"%user,
        tmp = raw_input()
        if tmp:
            user = tmp
        password = ''

    while not password:
        password = getpass('Password for {0}: '.format(user))

    scopes = ['user', 'repo']
    auth = authorize(user, password, scopes, id, 'http://nsclient.org')
    return auth.token

def github_auth(token=None, file=None, id=None, user=None, password=None):
    from github3 import login
    
    if not token:
        if file and os.path.exists(file):
            with open(file, 'r') as fd:
                token = fd.readline().strip()
    
    if not token:
        token = create_token(id, user, password)
        if file:
            with open(file, 'w') as fd:
                fd.write('%s\n'%token)

    print "Logging in to github..."
    return login(token=token)

def get_release(gh, release_name):
    repository = gh.repository('mickem', 'nscp')
    release = None
    for r in repository.iter_releases():
        if r.name == release_name:
            return r
    print "Creating release %s..."%release_name
    release = repository.create_release('%s'%release_name, 'master', release_name, 'PLEASE UPDATE!', True)

def upload_files(gh, release_name, files):
    names = map(lambda x:os.path.basename(x), files)
    if not files:
        print "No files specified use --files"
        return
    release = get_release(gh, release_name)

    for a in release.iter_assets():
        if a.name in names:
            print "Deleting old file %s..."%a.name
            a.delete()
    for f in files:
        print " + Uploading %s..."%f
        with open(f, "rb") as fd:
            release.upload_asset('application/zip', os.path.basename(f), fd.read())


parser = argparse.ArgumentParser(description='Upload files to github.')
parser.add_argument('--file', action='append',
                        help='the fils to upload')
parser.add_argument('--token', action='store',
                        help='The github auth token to use')
parser.add_argument('--auth-file', action='store',
                        help='A file to use for caching auth tokens')
parser.add_argument('--username', action='store',
                        help='Github username')
parser.add_argument('--token-id', action='store',
                        help='Used when creating new tokens as names')
parser.add_argument('--password', action='store',
                        help='Github password')
parser.add_argument('--release', action='store',
                        help='NSClient++ release')

args = parser.parse_args()

gh = github_auth(token=args.token, file=args.auth_file, id=args.token_id, user=args.username, password=args.password)

for i in xrange(3):
    try:
        print "Pushing to github attempt %d/3"%i
        upload_files(gh, args.release, args.file)
        break
    except Exception, e:
        print "Failed to upload: %s" %e
        pass