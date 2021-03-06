# setup ssh keys

import os
import paramiko
import logging
import util
import subprocess

logger = logging.getLogger('golddrive')
logging.getLogger("paramiko.transport").setLevel(logging.WARNING)

DIR = os.path.dirname(os.path.realpath(__file__))

def testhost(userhost, port=22):
	'''
	Test if host respond to port
	Return: True or False
	'''
	logger.debug(f'Testing port {port} at {userhost}...')
	user, host = userhost.split('@')
	client = paramiko.SSHClient()
	client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
	rb = util.ReturnBox()
	try:
		client.connect(hostname=host, username=user, 
				password='', port=port, timeout=5)	
		rb.returncode = util.ReturnCode.OK
	except (paramiko.ssh_exception.AuthenticationException,
		paramiko.ssh_exception.BadAuthenticationType,
		paramiko.ssh_exception.PasswordRequiredException):
		rb.returncode = util.ReturnCode.OK
	except Exception as ex:
		rb.returncode = util.ReturnCode.BAD_HOST
		rb.error = str(ex)
	finally:
		client.close()
	return rb

def testlogin(userhost, password, port=22):
	'''
	Test ssh password authentication
	'''
	logger.debug(f'Logging in with password for {userhost}...')
	rb = util.ReturnBox()
	if not password:
		rb.returncode =util.ReturnCode.BAD_LOGIN
		rb.error = 'Empty password'
		return rb

	user, host = userhost.split('@')
	client = paramiko.SSHClient()
	client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
	
	try:
		client.connect(hostname=host, username=user, 
				password=password, port=port, timeout=10, 
				look_for_keys=False)
		rb.returncode = util.ReturnCode.OK
	except (paramiko.ssh_exception.AuthenticationException,
		paramiko.ssh_exception.BadAuthenticationType,
		paramiko.ssh_exception.PasswordRequiredException) as ex:
		rb.returncode = util.ReturnCode.BAD_LOGIN
		rb.error = str(ex)
	except Exception as ex:
		rb.returncode = util.ReturnCode.BAD_HOST
		rb.error = str(ex)
	finally:
		client.close()
	return rb

def testssh(userhost, seckey, port=22):
	'''
	Test ssh key authentication
	'''
	logger.debug(f'Testing ssh keys for {userhost} using key {seckey}...')
	
	rb = testhost(userhost, port)
	if rb.returncode == util.ReturnCode.BAD_HOST:
		return rb
	
	if not os.path.exists(seckey):
		seckey_win = seckey.replace('/','\\')
		logger.error(f'Key does not exist: {seckey_win}')
		rb.returncode = util.ReturnCode.BAD_LOGIN
		rb.error = "No key"
		return rb

	cmd = f'''ssh.exe
		-i "{seckey}"
		-p {port} 
		-o PasswordAuthentication=no
		-o StrictHostKeyChecking=no 
		-o UserKnownHostsFile=/dev/null
		-o BatchMode=yes 
		{userhost} "echo ok"'''
	r = util.run(cmd, capture=True, timeout=10)
	
	if r.stdout == 'ok':
		# success
		rb.returncode = util.ReturnCode.OK
	elif 'Permission denied' in r.stderr:
		# wrong user or key issue
		rb.returncode = util.ReturnCode.BAD_LOGIN
		rb.error = 'Access denied'
	else:
		# wrong port: connection refused 
		# unknown host: connection timeout
		logger.error(r.stderr)
		rb.returncode = util.ReturnCode.BAD_HOST
		rb.error = r.stderr
	return rb

def generate_keys(seckey, userhost):
	logger.debug('Generating new ssh keys...')
	rb = util.ReturnBox()
	cmd = "ssh-keygen -N ''"

	# sk = paramiko.RSAKey.generate(2048)
	# try:
	# 	sshdir = os.path.dirname(seckey)
	# 	if not os.path.exists(sshdir):
	# 		os.makedirs(sshdir)
	# 		os.chmod(sshdir, 0o700)
	# 	sk.write_private_key_file(seckey)	
	# except Exception as ex:
	# 	logger.error(f'{ex}, {seckey}')
	# 	rb.error = str(ex)
	# 	return rb	
	
	# pubkey = f'ssh-rsa {sk.get_base64()} {userhost}'
	
	# try:
	# 	with open(seckey + '.pub', 'wt') as w:
	# 		w.write(pubkey)
	# except Exception as ex:
	# 	logger.error(f'Could not save public key: {ex}')

	rb.output = pubkey
	return rb

def has_app_keys(user):
	appkey = util.get_app_key(user)
	return os.path.exists(appkey)

def set_key_permissions(user, pkey):
	
	logger.debug('setting ssh key permissions...')
	ssh_folder = os.path.dirname(pkey)
	# Remove Inheritance ::
	# subprocess.run(fr'icacls {ssh_folder} /c /t /inheritance:d')
	util.run(fr'icacls {pkey} /c /t /inheritance:d', capture=True)
	
	# Set Ownership to Owner and SYSTEM account
	# subprocess.run(fr'icacls {ssh_folder} /c /t /grant %username%:F')
	util.run(fr'icacls {pkey} /c /t /grant { os.environ["USERNAME"] }:F', capture=True)
	util.run(fr'icacls {pkey} /c /t /grant SYSTEM:F', capture=True)
	
	# Remove All Users, except for Owner 
	# subprocess.run(fr'icacls {ssh_folder} /c /t /remove Administrator BUILTIN\Administrators BUILTIN Everyone System Users')
	util.run(fr'icacls {pkey} /c /t /remove Administrator BUILTIN\Administrators BUILTIN Everyone Users', capture=True)
	
	# Verify 
	# util.run(fr'icacls {pkey}')
	
def main(userhost, password, port=22):
	'''
	Setup ssh keys, return ReturnBox
	'''
	logger.debug(f'Setting up ssh keys for {userhost}...')
	rb = util.ReturnBox()

	# app key
	user, host = userhost.split('@')
	seckey = util.get_app_key(user)	

	# Check if keys need to be generated
	pubkey = ''
	if has_app_keys(user):
		logger.debug('Private key already exists.')
		sk = paramiko.RSAKey.from_private_key_file(seckey)
		pubkey = f'ssh-rsa {sk.get_base64()} {userhost}'
	else:
		rbkey = generate_keys(seckey, userhost)
		if rbkey.error:
			rbkey.returncode = util.ReturnCode.BAD_SSH
			return rbkey
		else:
			pubkey = rbkey.output

	# connect
	client = paramiko.SSHClient()
	client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

	rb.error = ''
	try:
		logger.debug('Connecting using password...')
		client.connect(hostname=host, username=user,
						password=password, port=port, timeout=10,
						look_for_keys=False)     
	except paramiko.ssh_exception.AuthenticationException:
		rb.error = f'User or password wrong'
		rb.returncode = 1
	except Exception as ex:
		rb.error = f'connection error: {ex}'
		rb.returncode = 2

	if rb.error:
		logger.error(rb.error)
		if 'getaddrinfo failed' in rb.error:
			rb.error = f'{host} not found'
		client.close()
		rb.returncode = util.ReturnCode.BAD_SSH
		return rb

	set_key_permissions(user, seckey)

	logger.debug(f'Publising public key...')
		
	# Copy to the target machines.
	# cmd = f"exec bash -c \"cd; umask 077; mkdir -p .ssh && echo '{pubkey}' >> .ssh/authorized_keys || exit 1\" || exit 1"
	cmd = f"exec sh -c \"cd; umask 077; mkdir -p .ssh; echo '{pubkey}' >> .ssh/authorized_keys\""
	logger.debug(cmd)
	ok = False
	
	try:
		stdin, stdout, stderr = client.exec_command(cmd, timeout=10)
		rc = stdout.channel.recv_exit_status()   
		if rc == 0:
			logger.debug('Key transfer successful')
			rb.returncode = util.ReturnCode.OK
		else:
			logger.error(f'Error transfering public key: exit {rc}, error: {stderr}')
	except Exception as ex:
		logger.error(ex)
		rb.returncode = util.ReturnCode.BAD_SSH
		rb.error = f'error transfering public key: {ex}'
		return rb
	finally:
		client.close()

	err = stderr.read()
	if err:
		logger.error(err)
		rb.returncode = util.ReturnCode.BAD_SSH
		rb.error = f'error transfering public key, error: {err}'
		return rb
	
	rb = testssh(userhost, seckey, port)
	if rb.returncode == util.ReturnCode.OK:
		rb.output = "SSH setup successfull." 
		logger.info(rb.output)
	else:
		message = 'SSH setup test failed'
		detail = ''
		if rb.returncode == util.ReturnCode.BAD_LOGIN:
			detail = ': authentication probem'
		else:
			message = ': connection problem'
		rb.error = message
		rb.returncode = util.ReturnCode.BAD_SSH
		logger.error(message + detail)
	return rb


if __name__ == '__main__':

	import sys
	import os
	import getpass
	assert (len(sys.argv) > 1 and
			'@' in sys.argv[1]) # usage: prog user@host
	os.environ['PATH'] = f'{DIR}\\..\\..\\openssh;' + os.environ['PATH']
	userhost = sys.argv[1]
	password = os.environ['GOLDDRIVE_PASS']
	port=22
	if ':' in userhost:
		userhost, port = userhost.split(':')                             
	logging.basicConfig(level=logging.INFO)

	main(userhost, password, port)


