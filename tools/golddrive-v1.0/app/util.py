# util module for running commands and load config
import os
import re
import subprocess
import logging
import yaml
import re
import getpass
import psutil
import shlex
from enum import Enum
import version


DIR = os.path.abspath(os.path.dirname(__file__))
logger = logging.getLogger('golddrive')

IPADDR = r'(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])'
HOSTNAME = r'(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\-]*[A-Za-z0-9])'
PATTERN_HOST = fr'^{HOSTNAME}|{IPADDR}$'
REGEX_HOST = re.compile(PATTERN_HOST)
CURRENT_USER = getpass.getuser()

class DriveStatus(Enum):
	CONNECTED = 1
	DISCONNECTED = 2
	IN_USE = 3
	BROKEN = 4
	NOT_SUPPORTED = 5
	NONE = -1

class Page(Enum):
	# match the stackwidget page index
	MAIN 	 = 0
	HOST 	 = 1
	LOGIN 	 = 2
	ABOUT 	 = 3

class ReturnCode(Enum):
	OK 			= 0
	BAD_DRIVE 	= 1
	BAD_HOST 	= 2
	BAD_LOGIN 	= 3
	BAD_SSH 	= 4
	BAD_MOUNT	= 5
	BAD_WINFSP	= 6
	NONE 		= -1

class ReturnBox():
	def __init__(self, out='', err=''):
		self.output = out
		self.error = err
		self.drive_status = None
		self.returncode = ReturnCode.NONE
		self.object = None

def load_config(path):
	try:
		with open(path) as f:
			js = str(f.read())
			js = re.sub(r'(?m)^[\t ]*//.*\n?', '', js)
			js = js.replace('\t','')
			config = yaml.load(js)
			for key in config:
				value = config[key]
				if r'%' in value:
					config[key] = os.path.expandvars(value)
			return config
	except Exception as ex:
		logger.error(f'Cannot read config file: {path}. Error: {ex}')
	return {}

def add_drive_config(**p):
	'''Add drive to config.json
	'''
	logger.debug('Adding drive to config file...')
	drive = p['drive']
	user, host = p['userhost'].split('@')
	port = p['port']
	drivename =	p['drivename']
	configfile = p['configfile']

	text = ('	"drives" : {\n'
			f'		"{drive}" : {{\n'
			f'			"drivename" : "{drivename}",\n'
			f'			"user" 		: "{user}",\n'
			f'			"port" 		: "{port}",\n'
			f'			"hosts" 	: ["{host}"],\n'
			'		},\n'
			'	}\n')
	try:
		with open(configfile) as r:
			lines = r.readlines()
		with open(configfile, 'w') as w:
			for line in lines:
				if r'"drives" : {}' in line:
					line = text
				w.write(line)
	except Exception as ex:
		logger.error(str(ex))

def get_app_key(user):
	sshdir = os.path.expandvars("%USERPROFILE%")
	seckey = fr'{sshdir}\.ssh\id_rsa-{user}-golddrive'
	return seckey.replace(f'\\', '/')

def rich_text(text):
	t = text.replace('\n','<br/>') #.replace('\'','\\\'')
	return f'<html><head/><body><p>{t}</p></body></html>'

def make_hyperlink(href, text):
	
	return f"<a href='{href}'><span style=\"text-decoration: none; color:#0E639C;\">{text}</span></a>"

def get_user_host_port(text):
	userhostport = text
	userhost = text
	host = text
	port = None
	user = CURRENT_USER
	if ':' in text:
		userhost, port = text.split(':')
		host = userhost
	if '@' in userhost:
		user, host = userhost.split('@')
	if not REGEX_HOST.match(host):
		host = '<invalid>'
	if not user:
		user = '<invalid>'
	if port:
		try:
			port = int(port)
			if port < 0 or port > 65635:
				raise
		except:
			port = '<invalid>'
	else:
		port = 22
	return user, host, port

def run(cmd, capture=False, detach=False, shell=True, timeout=30):
	cmd = re.sub(r'[\n\r\t ]+',' ', cmd).replace('  ',' ').strip()
	header = 'CMD'
	if shell:
		header += ' (SHELL)'
	logger.debug(f'{header}: {cmd}')

	r = subprocess.CompletedProcess(cmd, 0)
	r.stdout = ''
	r.stderr = ''

	try:
		if detach:
			CREATE_NEW_PROCESS_GROUP = 0x00000200
			DETACHED_PROCESS = 0x00000008
			# p = subprocess.Popen(shlex.split(cmd), 
			# 		stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
			# 		creationflags=DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP)
			p = subprocess.Popen(shlex.split(cmd), close_fds=True,
					creationflags=DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP)
			logger.debug(f'Detached process {p.pid} started.')
		else:
			r = subprocess.run(cmd, capture_output=capture, shell=shell, 
					timeout=timeout, text=True)
	except Exception as ex:
		r.stderr = repr(ex)
		logger.error(r)
		return r

	if r.returncode != 0:
		if r.stderr and r.stderr.startswith('Warning'):
			logger.warning(r)
		else:
			logger.error(r)		
	# else:
	# 	logger.debug(r)
	if capture:
		r.stdout = r.stdout.strip()
		r.stderr = r.stderr.strip()
	return r
	
def get_app_version():
	try:
		path = os.environ['GOLDDRIVE'] + '\\version.txt'
		if not os.path.exists(path):
			path = os.environ['GOLDDRIVE'] + '\\..\\version.txt'			
		with open(path) as r:
			return r.read().strip()
	except:
		return ''

def is_winfsp_installed():
	return get_winfsp_version() != None

def get_winfsp_version():
	winfsp_dll = os.path.expandvars(fr'%ProgramFiles(x86)%\WinFsp\bin\winfsp-x64.dll')
	return version.get_file_version(winfsp_dll);
	
def get_versions():
	ssh = ''
	golddrive = ''
	winfsp = ''
	
	r = run(f'ssh -V', capture=True)
	if r.returncode == 0:
		for c in r.stderr.split(','):
			if c.strip():
				ssh += c.strip().replace('  ',' ') + '\n'
		ssh = ssh.strip()
		
	r = run(f'golddrive --version', capture=True)
	if r.returncode == 0:
		golddrive = fr"{r.stderr}"
	
	winfsp = get_winfsp_version()
	if not winfsp:
		winfsp = 'N/A'
	if winfsp:
		winfsp = f'WinFSP {winfsp}'
	result = f'{ssh}\n{golddrive}\n{winfsp}'
	return result

def set_path(path=None):
	# print('setting path...')
	golddrive = os.path.realpath(fr'{DIR}\..')
	os.environ['GOLDDRIVE'] = golddrive
	path = [
		# fr'{golddrive}\lib\lib\PyQt5\Qt\bin',
		fr'{golddrive}',
		fr'{golddrive}\bin',
		fr'{golddrive}\bin\sshfs\bin',
		fr'{golddrive}\python',
		fr'{golddrive}\python\lib',
		fr'C:\Windows',
		fr'C:\Windows\system32',
		# fr'C:\Windows\System32\Wbem',		
	]
	os.environ['PATH'] = ';'.join(path)
	logger.debug('PATH:')
	for p in os.environ['PATH'].split(';'):
		logger.debug(p)
	# print('sys.path:')
	# for p in sys.path:
	# 	print(p)

def taskkill(plist, timeout=5):
	def on_terminate(p):
		if p.returncode != 0:
			if p.returncode == 15:
				logger.debug(f"Process {p.pid} terminated")
			else:
				logger.error(f"Process {p.pid} terminated with exit code {p.returncode}")

	for p in plist:
		logger.debug(f'Terminating process {p.pid}...')
		p.terminate()
	gone, alive = psutil.wait_procs(plist, timeout=timeout, callback=on_terminate)
	if alive:
		# send SIGKILL
		for p in alive:
			logger.error(f"Process {p.pid} survived SIGTERM; trying SIGKILL")
			p.kill()
		gone, alive = psutil.wait_procs(alive, timeout=timeout, callback=on_terminate)
		if alive:
			# give up
			for p in alive:
				logger.error(f"Process {p.pid} survived SIGKILL; giving up")
				return False
	return True

def restart_explorer():
	plist = [p for p in psutil.process_iter(attrs=['name']) if 'explorer.exe' in p.info['name']]
	taskkill(plist)

	# no need to restart, as p.terminate() restarts explorer?
	# util.run(fr'start /b c:\windows\explorer.exe', capture=True)

def kill_drive(drive):
	logger.debug(f'Killing drive {drive} process...')
	plist = []
	drive = drive.lower()
	for p in psutil.process_iter(attrs=['cmdline']):
		if p.info['cmdline']:
			cmdline = ' '.join(p.info['cmdline']).lower()
			# print(f'cmdline: {cmdline}')
			if 'golddrive.exe' in cmdline and f' {drive} ' in cmdline:
				plist.append(p)
	taskkill(plist)
	
	# wmic is not working in some machines			
	# cmd = f"""wmic process where (commandline like '% {drive} %' 
	# 	and name='sshfs.exe') get processid"""
	# r = util.run(cmd, capture=True)
	# if r.returncode == 0 and r.stdout:
	# 	pid = r.stdout.split('\n')[-1]
	# 	return pid
	# else:
	# 	return '0'


if __name__ == '__main__':

	logging.basicConfig(level=logging.INFO)
	run('tasklist')	
	run(fr'more "%USERPROFILE%\.ssh\id_rsa"')	
	run(fr'echo "hello world"')