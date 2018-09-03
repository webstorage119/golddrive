#!/usr/bin/python3
# -*- coding: utf-8 -*-
import sys
import os
import time
import json
import logging
import subprocess
import getpass
from util import ReturnBox
from PyQt5 import QtCore, QtGui
from PyQt5.QtWidgets import (QWidget, QLabel, 
	QComboBox, QApplication, QMainWindow, QMenu)
from app_ui import Ui_MainWindow

DIR = os.path.abspath(os.path.dirname(__file__))
# logging.basicConfig(filename=fr'{DIR}\..\ssh-drive.log', level=logging.INFO)
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger('ssh-drive')



class Worker(QtCore.QObject):

	workDone = QtCore.pyqtSignal(str, ReturnBox)
	
	def get_drive_status(sefl, p):
		rb = ReturnBox('','Not implemented')
		return rb

	def testssh(self, p):
		import setupssh	
		ok = setupssh.testssh(p['ssh'],p['user'],p['host'], p['port'])
		rb = ReturnBox()
		if ok:
			rb.output = "SSH authentication is OK"
		return rb

	def setupssh(self, p):
		import setupssh	
		return setupssh.main(p['ssh'], p['user'], p['host'], p['password'], p['port'])

	def restart_explorer(self, p):
		import unmount
		unmount.restart_explorer()
		msg = 'Explorer.exe was restarted.'
		return ReturnBox('','Not implemented')

	def connect(self, p):
		import mount		
		# mount.main(sshfs, drive, userhost, drivename)
		msg =  'Drive is connected'
		return ReturnBox('','Not implemented')

	def disconnect(self, p):
		import mount		
		# mount.main(sshfs, drive, userhost, drivename)
		msg =  'Drive is connected'
		return ReturnBox('','Not implemented')

	def reconnect(self, p):
		import mount		
		# mount.main(sshfs, drive, userhost, drivename)
		msg =  'Drive is connected'
		return ReturnBox('','Not implemented')

	# slot decorator is optional, used here for documenting argument's type
	@QtCore.pyqtSlot(str, dict)
	def work(self, task, param):
		time.sleep(0.5)
		if hasattr(self, task):
			rb = getattr(self, task)(param)
		else:
			rb = ReturnBox('','Not implemented')
		self.workDone.emit(task, rb)


class Window(Ui_MainWindow, QMainWindow):
	
	workRequested = QtCore.pyqtSignal(str, dict)

	def __init__(self):
		super().__init__()
		self.loaded = False
		self.setupUi(self)
		self.setWindowTitle('SSH Drive')
		app_icon = QtGui.QIcon()
		app_icon.addFile(fr'{DIR}\images\icon_16.png', QtCore.QSize(16,16))
		app_icon.addFile(fr'{DIR}\images\icon_32.png', QtCore.QSize(32,32))
		app_icon.addFile(fr'{DIR}\images\icon_48.png', QtCore.QSize(48,48))
		QApplication.setWindowIcon(app_icon)
		# widgets = self.findChildren(QWidget)
		# for w in widgets:
		# 	w.setProperty('css',True)
		os.chdir(f'{DIR}')
		with open(f'{DIR}/style.css') as r:
			self.setStyleSheet(r.read())

		# read config.json
		with open(fr'{DIR}\..\config.json') as f:
			self.config = json.load(f)
		print (self.config)
		# store parameters to send to worker
		self.param = {}
		self.param['user'] = getpass.getuser()
		self.param['ssh'] = fr"{self.config['sshfs_path']}\ssh.exe"
		self.param['sshfs'] = fr"{self.config['sshfs_path']}\sshfs.exe"
		
		self.settings = QtCore.QSettings("sganis", "ssh-drive")
		if self.settings.value("geometry"):
			self.restoreGeometry(self.settings.value("geometry"))
			self.restoreState(self.settings.value("windowState"))
		# load combo
		items = []
		drives = self.config['drives']
		for d in drives:
			items.append(f"   {d}   {drives[d]['name']}")
		self.cboParam.addItems(items)
		self.lblUserHostPort.setText("")
		self.cboParam.setCurrentIndex(self.settings.value("cboParam",0))
		self.progressBar.setVisible(False)	
		self.lblStatus.setText("")
		self.widgetPassword.setVisible(False)

		menu = QMenu()
		menu.addAction('Setup SSH keys', self.mnuSetupSsh)
		menu.addAction('Reconnect', self.mnuReconnect)
		menu.addAction('Open settings folder', self.mnuSettings)
		menu.addAction('Show log file', self.mnuShowLog)
		menu.addAction('Restart Explorer.exe', self.mnuRestartExplorer)
		self.pbHamburger.setMenu(menu)

		# worker thread
		self.thread = QtCore.QThread()
		self.worker = Worker()
		self.worker.moveToThread(self.thread)
		self.workRequested.connect(self.worker.work)
		self.worker.workDone.connect(self.onWorkDone)
		self.thread.start()
		self.loaded = True

	@QtCore.pyqtSlot(str, ReturnBox)
	def onWorkDone(self, task, rb):
		if task == 'testssh':
			if rb.output:
				self.end(rb.output)
			else:
				self.setupSsh('Authentication required:')
		elif task == 'setupssh' and 'password wrong' in rb.error:
				self.setupSsh('Wrong password, try again:')
		else:
			msg = rb.output if not rb.error else rb.error
			self.end(msg)
			self.lblStatus.setProperty("error", rb.error != '');
			self.lblStatus.style().unpolish(self.lblStatus);
			self.lblStatus.style().polish(self.lblStatus);

	# decorator used to trigger only the int overload and not twice
	@QtCore.pyqtSlot(int)
	def on_cboParam_currentIndexChanged(self, e):
		# print(f'index changed: {e}')
		cbotext = self.cboParam.currentText();
		drive = cbotext.split()[0].strip()
		d = self.config['drives'][drive]
		self.param['drive'] = drive
		self.param['host'] = d['hosts'][0]
		self.param['port'] = d.get('port', 22)
		userhostport = f"{self.param['user']}@{self.param['host']}:{self.param['port']}"
		self.lblUserHostPort.setText(userhostport)
		if self.loaded:
			self.start(f"Checking {self.param['drive']}...")
			self.workRequested.emit('get_drive_status', self.param)

	def mnuSetupSsh(self):
		user = self.param['user']
		host = self.param['host']
		port = self.param['port']
		self.start(f'Checking ssh keys for {user}...')
		self.workRequested.emit('testssh', self.param)
	
	def setupSsh(self, msg):
		self.progressBar.setVisible(False)
		self.lblPassword.setText(msg)
		self.txtPassword.setFocus()
		self.lblStatus.setVisible(False)
		self.widgetPassword.setVisible(True)
	
	def on_txtPassword_returnPressed(self):
		self.widgetPassword.setVisible(False)
		self.lblStatus.setVisible(True)
		self.start('Setting up ssh authentication...')
		self.param['password'] = str(self.txtPassword.text())
		self.workRequested.emit('setupssh', self.param)

	def mnuReconnect(self):
		self.start('Reconnecting drive...')
		self.workRequested.emit('reconnect', self.param)

	def mnuSettings(self):
		self.start('Open settings folder...')
		self.workRequested.emit('settings', self.param)

	def mnuShowLog(self):
		self.start('Show log file...')
		self.workRequested.emit('show_log', self.param)
	
	def mnuRestartExplorer(self):
		self.start('Restarting explorer...')
		self.workRequested.emit('restart_explorer', self.param)
		
	def closeEvent(self, event):	
		self.settings.setValue("geometry", self.saveGeometry())
		self.settings.setValue("windowState", self.saveState())
		self.settings.setValue("cboParam", self.cboParam.currentIndex())	
		QMainWindow.closeEvent(self, event)
		self.thread.quit()
		self.thread.wait()

	def start(self, message):	
		self.lblStatus.setProperty("error", False);
		self.lblStatus.style().unpolish(self.lblStatus);
		self.lblStatus.style().polish(self.lblStatus);	
		self.lblStatus.setText(message)
		self.progressBar.setVisible(True)
		self.pbConnect.setEnabled(False)
		self.widgetPassword.setVisible(False)
		self.lblStatus.setVisible(True)
		self.txtPassword.setText("")

	def end(self, message=''):
		self.lblStatus.setText(message)
		self.progressBar.setVisible(False)
		self.pbConnect.setEnabled(True)
		# self.pbConnect.setText('CONNECT')

	def on_pbConnect_released(self):
		drive = self.param['drive']
		self.start(f'Connecting {drive} ...')
		self.workRequested.emit('connect', self.param)

	def on_pbCancel_released(self):
		self.widgetPassword.setVisible(False)
		self.lblStatus.setVisible(True)
		self.txtPassword.setText('')
		self.end()

	def on_lblSettings_linkActivated(self, link):

		subprocess.call(fr'start /b c:\windows\explorer.exe "{DIR}\.."', shell=True)



def run():

	app = QApplication(sys.argv)
	window = Window()
	window.show()
	sys.exit(app.exec_())



if __name__ == '__main__':
	run()
