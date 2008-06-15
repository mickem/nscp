/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "stdafx.h"
#include "WEBConfiguration.h"
#include <strEx.h>
#include <time.h>

#include <WApplication>
#include <WBreak>
#include <WContainerWidget>
#include <WViewWidget>
#include <WLineEdit>
#include <WPushButton>
#include <WText>
#include <WCheckBox>
#include <WPanel>
#include <WTable>
#include <WTabWidget>
#include <WTreeNode>
#include <WIconPair>

#include "PanelList.h"

#include <boost/thread.hpp>

#include <web/Configuration.h>
#include <web/HTTPStream.h>
#include <web/WebController.h>
#include <web/http/Configuration.h>
#include <web/http/Server.h>

WEBConfiguration gWEBConfiguration;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

WEBConfiguration::WEBConfiguration() {
}
WEBConfiguration::~WEBConfiguration() {
}

/*
* A simple hello world application class which demonstrates how to react
* to events, read input, and give feed-back.
*/
class HelloApplication : public Wt::WContainerWidget /*: public Wt::WApplication*/
{
public:
	HelloApplication(Wt::WContainerWidget *parent);

private:
	Wt::WLineEdit *nameEdit_;
	Wt::WText *greeting_;

	void greet();
	void save();
	void checkPlugin();
	Wt::WWidget *helloWorldExample();
	Wt::WWidget *modulesPane();
	Wt::WWidget *configurationPane();
	Wt::WWidget* wrapViewOrDefer(Wt::WWidget *(HelloApplication::*createWidget)());

};


/*
* A utility container widget which defers creation of its single
* child widget until the container is loaded (which is done on-demand
* by a WMenu). The constructor takes the create function for the
* widget as a parameter.
*
* We use this to defer widget creation until needed, which is used
* for the Treelist example tab.
*/
template <typename Function>
class DeferredWidget : public Wt::WContainerWidget
{
public:
	DeferredWidget(Function f)
		: f_(f) { }

private:
	void load() {
		std::cout << "--- load --" << std::endl;
		addWidget(f_());
	}

	Function f_;
};

template <typename Function>
DeferredWidget<Function> *deferCreate(Function f)
{
	return new DeferredWidget<Function>(f);
}


void addHeader(Wt::WTable *t, const char *value) {
	t->elementAt(0, t->numColumns())->addWidget(new Wt::WText(value));    
}


Wt::WWidget *HelloApplication::helloWorldExample()
{
	Wt::WContainerWidget *result = new Wt::WContainerWidget();
	new Wt::WText("home.examples.treelist", result);
	return result;
}

Wt::WWidget *HelloApplication::modulesPane()
{
	Wt::WContainerWidget *result = new Wt::WContainerWidget();
	new Wt::WText("modulesPabe", result);
	Wt::WTable *moduleList = new Wt::WTable(result);

	::addHeader(moduleList, "Module");
	::addHeader(moduleList, "Description");

	Wt::WPushButton *saveBtn = new Wt::WPushButton("Save", result);
	saveBtn->clicked.connect(SLOT(this, HelloApplication::save));

	NSCModuleHelper::plugin_info_list pList = NSCModuleHelper::getPluginList();
	int i=1;

	for (NSCModuleHelper::plugin_info_list::const_iterator cit = pList.begin(); cit != pList.end();++cit,i++) {
		Wt::WCheckBox *cb = new Wt::WCheckBox(strEx::wstring_to_string((*cit).name), moduleList->elementAt(i, 0));

		if (NSCModuleHelper::getSettingsString(MAIN_MODULES_SECTION, (*cit).dll , _T("disabled")) == _T("disabled")) {
			cb->setChecked(false);
		} else {
			cb->setChecked(true);
		}
		cb->setId(strEx::wstring_to_string((*cit).dll));
		cb->clicked.connect(SLOT(this, HelloApplication::checkPlugin));
		new Wt::WText(strEx::wstring_to_string((*cit).description), moduleList->elementAt(i, 1));

		if (i%2==0)
			moduleList->rowAt(i)->setStyleClass("odd");
	}
	return result;
}


Wt::WTreeNode *createExampleNode(const Wt::WString& label, Wt::WTreeNode *parentNode)
{
	Wt::WIconPair *labelIcon
		= new Wt::WIconPair("icons/document.png", "icons/document.png", false);

	Wt::WTreeNode *node = new Wt::WTreeNode(label, labelIcon, parentNode);
	node->label()->setFormatting(Wt::WText::PlainFormatting);
	//node->label()->clicked.connect(this, f);

	return node;
}

Wt::WWidget *HelloApplication::configurationPane()
{
	Wt::WContainerWidget *result = new Wt::WContainerWidget();
	Wt::WContainerWidget *div = new Wt::WContainerWidget(result);


	Wt::WIconPair *mapIcon
		= new Wt::WIconPair("icons/yellow-folder-closed.png",
		"icons/yellow-folder-open.png", false);

	Wt::WTreeNode *rootNode = new Wt::WTreeNode("Examples", mapIcon);
	rootNode->setImagePack("icons/");
	rootNode->expand();
	rootNode->setLoadPolicy(Wt::WTreeNode::NextLevelLoading);
	createExampleNode("test", rootNode);
	createExampleNode("test", rootNode);
	createExampleNode("test", rootNode);
	createExampleNode("test", rootNode);

	div->addWidget(rootNode);

	//createExampleNode("Menu & ToolBar", rootNode,
	//	&ExtKitchenApplication::menuAndToolBarExample);

	//new Wt::WText("config pane", result);

	return result;
}

Wt::WWidget* HelloApplication::wrapViewOrDefer(Wt::WWidget *(HelloApplication::*createWidget)())
{
	/*
	* We can only create a view if we have javascript for the client-side
	* tree manipulation -- otherwise we require server-side event handling
	* which is not possible with a view since the server-side widgets do
	* not exist. Otherwise, all we can do to avoid unnecessary server-side
	* resources is deferring creation until load time.
	*/
	if (Wt::wApp->environment().javaScript())
		return makeStaticModel(boost::bind(createWidget, this));
	else
		return deferCreate(boost::bind(createWidget, this));
}
/*
* The env argument contains information about the new session, and
* the initial request. It must be passed to the WApplication
* constructor so it is typically also an argument for your custom
* application constructor.
*/
/*
HelloApplication::HelloApplication(const Wt::WEnvironment& env)
: Wt::WApplication(env)
*/
HelloApplication::HelloApplication(Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent)
{
	//setTitle("NSClient++ Configuratio");

	Wt::WTabWidget *exampleTabs = new Wt::WTabWidget(parent);
	exampleTabs->enableBrowserHistory("example");

	exampleTabs->addTab(wrapViewOrDefer(&HelloApplication::helloWorldExample), "Welcome");
	//exampleTabs->addTab(wrapViewOrDefer(&HelloApplication::modulesPane), "Modules2");
	//exampleTabs->addTab(deferCreate(boost::bind(&HelloApplication::modulesPane, this)), "Modules3");
	exampleTabs->addTab(modulesPane(), "Modules");
	exampleTabs->addTab(configurationPane(), "Configuration");
	//exampleTabs->addTab(deferCreate(boost::bind(&HelloApplication::helloWorldExample, this)), "Configuration");

	//exampleTabs->addTab(deferCreate(boost::bind(&Home::treelistExample, this)),"Treelist");

	//useStyleSheet("charts.css");

	/*
	* Connect signals with slots
	*/
	//b->clicked.connect(SLOT(this, HelloApplication::greet));
	//nameEdit_->enterPressed.connect(SLOT(this, HelloApplication::greet));
}

void HelloApplication::greet()
{
	/*
	* Update the text, using text input into the nameEdit_ field.
	*/
	greeting_->setText("Hello there, " + nameEdit_->text());
}
void HelloApplication::save()
{
	NSCModuleHelper::settings_save();
}

void HelloApplication::checkPlugin()
{
	Wt::WCheckBox *cb = dynamic_cast<Wt::WCheckBox*>(sender());
	if (cb->isChecked())
		NSCModuleHelper::SetSettingsString(MAIN_MODULES_SECTION, strEx::string_to_wstring(cb->id()), _T(""));
	else
		NSCModuleHelper::SetSettingsString(MAIN_MODULES_SECTION, strEx::string_to_wstring(cb->id()), _T("disabled"));
	NSC_DEBUG_MSG_STD(strEx::string_to_wstring(cb->id()) + _T(": ") + strEx::itos(cb->isChecked()));
}


/*
Wt::WApplication *createApplication(const Wt::WEnvironment& env)
{*/
	/*
	* You could read information from the environment to decide whether
	* the user has permission to start a new application
	*/
/*
	return new HelloApplication(env);
}
*/
namespace {

	typedef std::vector<Wt::EntryPoint> EntryPointList;
	EntryPointList entryPoints;

}

Wt::WApplication *createApplication(const Wt::WEnvironment& env)
{
	Wt::WApplication *app = new Wt::WApplication(env);

	app->messageResourceBundle().use("wt-home", false);
	app->useStyleSheet("images/wt.css");
	app->useStyleSheet("home.css");
	app->setTitle("Wt, C++ Web Toolkit");

	new HelloApplication(app->root());
	return app;
}


bool WEBConfiguration::loadModule(NSCAPI::moduleLoadMode mode) {
	if (mode == NSCAPI::normalStart) {
		char **args = new char*[10];
		for (int i=0;i<10;i++) {
			args[i] = new char[1024];
		}
		strcpy(args[0],"");
		strcpy(args[1],"--http-address=0.0.0.0");
		strcpy(args[2],"--http-port=8080");
		strcpy(args[3],"--deploy-path=/hello");
		strcpy(args[4],"--docroot=.");
		strcpy(args[5],"");

		//WRun(5, (char**)args, &createApplication);
		Wt::ApplicationCreator cApplication = createApplication;

		// We don't want to terminate until we are completely started up
		//boost::mutex::scoped_lock terminationLock(terminationMutex);

		//if (createApplication)
		entryPoints.push_back(Wt::EntryPoint(Wt::WebSession::Application, cApplication, std::string()));

		wtConfiguration = new Wt::Configuration(5, (char**)args, entryPoints, Wt::Configuration::WtHttpdServer);
		stream = new Wt::HTTPStream();
		controller = new Wt::WebController(*wtConfiguration, *stream);

		try	{

				config = new http::server::Configuration(5, (char**)args);

				// Override sessionIdPrefix setting
				if (!config->sessionIdPrefix().empty())
					wtConfiguration->setSessionIdPrefix(config->sessionIdPrefix());

				// Set default entry point
				wtConfiguration->setDefaultEntryPoint(config->deployPath().substr(1));

				// Run server in background thread.
				server = new http::server::Server(*config, *wtConfiguration);
				//server = new http::server::Server();

				//NUM_THREADS = config.threads();
	#define NUM_THREADS 1
				threads = new boost::thread *[NUM_THREADS];
	
				for (int i = 0; i < NUM_THREADS; ++i)
					threads[i] = new boost::thread(boost::bind(&http::server::Server::run, server));
	

				// Set console control handler to allow server to be stopped.
				//console_ctrl_function = boost::bind(&http::server::Server::stop, &s);
				//SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

				// Nothing to do here - just wait until program termination is requested,
				// e.g. by ctrl-c in the terminal
				//terminationCondition.wait(terminationLock);

			} catch (asio_system_error& e) {
				std::cerr << "asio error: " << e.what() << "\n";
			} catch (http::server::Configuration::Exception& e) {
				std::cerr << e.what() << "\n";
				return 1;
			} catch (std::exception& e) {
				std::cerr << "exception: " << e.what() << "\n";
			}
	}
	return true;
}
bool WEBConfiguration::unloadModule() {
	return true;
}

bool WEBConfiguration::hasCommandHandler() {
	return true;
}
bool WEBConfiguration::hasMessageHandler() {
	return false;
}

// set writeConf type
NSCAPI::nagiosReturn WEBConfiguration::writeConf(const unsigned int argLen, TCHAR **char_args, std::wstring &message) {
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);

	if (args.size() > 0) {
		if (args.front() == _T("reg")) {
			if (NSCModuleHelper::WriteSettings(NSCAPI::settings_registry) == NSCAPI::isSuccess) {
				message = _T("Settings written successfully.");
				return NSCAPI::returnOK;
			}
			message = _T("ERROR could not write settings.");
			return NSCAPI::returnCRIT;
		}
	}
	if (NSCModuleHelper::WriteSettings(NSCAPI::settings_inifile) == NSCAPI::isSuccess) {
		message = _T("Settings written successfully.");
		return NSCAPI::returnOK;
	}
	message = _T("ERROR could not write settings.");
	return NSCAPI::returnCRIT;
}

NSCAPI::nagiosReturn WEBConfiguration::readConf(const unsigned int argLen, TCHAR **char_args, std::wstring &message) {
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);

	if (args.size() > 0) {
		if (args.front() == _T("reg")) {
			if (NSCModuleHelper::ReadSettings(NSCAPI::settings_registry) == NSCAPI::isSuccess) {
				message = _T("Settings written successfully.");
				return NSCAPI::returnOK;
			}
			message = _T("ERROR could not write settings.");
			return NSCAPI::returnCRIT;
		}
	}
	if (NSCModuleHelper::ReadSettings(NSCAPI::settings_inifile) == NSCAPI::isSuccess) {
		message = _T("Settings written successfully.");
		return NSCAPI::returnOK;
	}
	message = _T("ERROR could not write settings.");
	return NSCAPI::returnCRIT;
}
// set setVariable int <section> <variable> <value>
NSCAPI::nagiosReturn WEBConfiguration::setVariable(const unsigned int argLen, TCHAR **char_args, std::wstring &message) {
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.size() < 3) {
		message = _T("Invalid syntax.");
		return NSCAPI::returnUNKNOWN;
	}
	std::wstring type = args.front(); args.pop_front();
	std::wstring section = args.front(); args.pop_front();
	std::wstring key = args.front(); args.pop_front();
	std::wstring value;
	if (args.size() >= 1) {
		value = args.front();
	}
	if (type == _T("int")) {
		NSCModuleHelper::SetSettingsInt(section, key, strEx::stoi(value));
		message = _T("Settings ") + key + _T(" saved successfully.");
		return NSCAPI::returnOK;
	} else if (type == _T("string")) {
		NSCModuleHelper::SetSettingsString(section, key, value);
		message = _T("Settings ") + key + _T(" saved successfully.");
		return NSCAPI::returnOK;
	} else {
		NSCModuleHelper::SetSettingsString(type, section, key);
		message = _T("Settings ") + section + _T(" saved successfully.");
		return NSCAPI::returnOK;
	}
}
NSCAPI::nagiosReturn WEBConfiguration::getVariable(const unsigned int argLen, TCHAR **char_args, std::wstring &message) {
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.size() < 2) {
		message = _T("Invalid syntax.");
		return NSCAPI::returnUNKNOWN;
	}
	std::wstring section = args.front(); args.pop_front();
	std::wstring key = args.front(); args.pop_front();
	std::wstring value;
	value = NSCModuleHelper::getSettingsString(section, key, _T(""));
	message = section+_T("/")+key+_T("=")+value;
	return NSCAPI::returnOK;
}
int WEBConfiguration::commandLineExec(const TCHAR* command,const unsigned int argLen,TCHAR** args) {
	std::wstring str;
	if (_wcsicmp(command, _T("setVariable")) == 0) {
		setVariable(argLen, args, str);
	} else if (_wcsicmp(command, _T("writeConf")) == 0) {
		writeConf(argLen, args, str);
	} else if (_wcsicmp(command, _T("getVariable")) == 0) {
		setVariable(argLen, args, str);
	} else {
		std::wcout << _T("Unsupported command: ") << command << std::endl;
	}
	return 0;
}


NSCAPI::nagiosReturn WEBConfiguration::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) {
	if (command == _T("setVariable")) {
		setVariable(argLen, char_args, msg);
		return NSCAPI::returnOK;
	} else if (command == _T("getVariable")) {
		getVariable(argLen, char_args, msg);
		return NSCAPI::returnOK;
	} else if (command == _T("readConf")) {
		return readConf(argLen, char_args, msg);
	} else if (command == _T("writeConf")) {
		return writeConf(argLen, char_args, msg);
	}	
	return NSCAPI::returnIgnored;
}


NSC_WRAPPERS_MAIN_DEF(gWEBConfiguration);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gWEBConfiguration);
NSC_WRAPPERS_CLI_DEF(gWEBConfiguration);

