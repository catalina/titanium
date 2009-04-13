/** * Appcelerator Titanium - licensed under the Apache Public License 2
 * see LICENSE in the root folder for details on the license.
 * Copyright (c) 2008 Appcelerator, Inc. All Rights Reserved.
 */

#include "installer.h"
#include "titanium_icon.h"

#include <iostream>
#include <sstream>

void *download_thread_f(gpointer data);
void *install_thread_f(gpointer data);
static gboolean watcher(gpointer data);
static void install_cb(GtkWidget *widget, gpointer data);
static void cancel_cb(GtkWidget *widget, gpointer data);
static void destroy_cb(GtkWidget *widget, gpointer data);
static int do_install_sudo();

#define LICENSE_WINDOW_WIDTH 600
#define LICENSE_WINDOW_HEIGHT 500
#define NO_LICENSE_WINDOW_WIDTH 400 
#define NO_LICENSE_WINDOW_HEIGHT 150
#define PROGRESS_WINDOW_WIDTH 350 
#define PROGRESS_WINDOW_HEIGHT 100

#define CANCEL 0
#define HOMEDIR_INSTALL 1
#define SYSTEM_INSTALL 2

Installer* Installer::instance;
string systemRuntimeHome;
string userRuntimeHome;
string temporaryDirectory;
int argc;
char** argv;

Installer::Installer(string applicationPath, vector<Job*> jobs, int installType) :
	applicationPath(applicationPath),
	jobs(jobs),
	app(NULL),
	installType(installType),
	stage(PREDOWNLOAD),
	currentJob(NULL),
	cancel(false),
	error("")
{
	Installer::instance = this;
	this->app = BootUtils::ReadManifest(applicationPath);

	this->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	string title = this->app->name + " Installer";
	gtk_window_set_title(GTK_WINDOW(this->window), title.c_str());
	g_signal_connect(
		G_OBJECT(this->window),
		"destroy",
		G_CALLBACK(destroy_cb),
		(gpointer) this);

	if (this->installType == HOMEDIR_INSTALL)
	{
		this->CreateIntroView();
	}
	else
	{
		this->StartDownloading();
	}

	gdk_threads_enter();
	int timer = g_timeout_add(100, watcher, this);
	gtk_main();
	g_source_remove(timer);
	gdk_threads_leave();
}

void Installer::FinishInstall()
{
	if (this->stage == SUCCESS && this->installType == HOMEDIR_INSTALL)
	{
		FILE *file;
		string path = FileUtils::Join(this->app->path.c_str(), ".installed", NULL);
		file = fopen(path.c_str(),"w"); 
		fprintf(file, "\n");
		fclose(file);
	}
}

Installer::~Installer()
{
	std::vector<Job*>::iterator i = jobs.begin();
	while (i != jobs.end())
	{
		Job* j = *i;
		i = jobs.erase(i);
		delete j;
	}
}

void Installer::ResizeWindow(int width, int height)
{
	// Try very hard to center the window
	gtk_window_set_default_size(GTK_WINDOW(this->window), width, height);
	gtk_window_resize(GTK_WINDOW(this->window), width, height);
	gtk_window_set_gravity(GTK_WINDOW(this->window), GDK_GRAVITY_CENTER);
	gtk_window_move(
		GTK_WINDOW(this->window),
		gdk_screen_width()/2 - width/2,
		gdk_screen_height()/2 - height/2);
}

void Installer::CreateInfoBox(GtkWidget* vbox)
{
	// Create the top part with the application icon and information
	GtkWidget* infoVbox = gtk_vbox_new(FALSE, 2);

	string nameLabelText = "<span size=\"xx-large\">";
	nameLabelText.append(this->app->name);
	nameLabelText.append("</span>");
	GtkWidget* nameLabel = gtk_label_new(nameLabelText.c_str());
	gtk_label_set_use_markup(GTK_LABEL(nameLabel), TRUE);
	gtk_misc_set_alignment(GTK_MISC(nameLabel), 0.0, 0.0);
	gtk_box_pack_start(GTK_BOX(infoVbox), nameLabel, FALSE, FALSE, 0);
	if (!this->app->publisher.empty())
	{
		string publisherLabelText = string("Publisher: ") + this->app->publisher;
		GtkWidget* publisherLabel = gtk_label_new(publisherLabelText.c_str());
		gtk_misc_set_alignment(GTK_MISC(publisherLabel), 0.0, 0.0);
		gtk_box_pack_start(GTK_BOX(infoVbox), publisherLabel, FALSE, FALSE, 0);
	}
	if (!this->app->url.empty())
	{
		string urlLabelText = string("From: ") + this->app->url;
		GtkWidget* urlLabel = gtk_label_new(urlLabelText.c_str());
		gtk_misc_set_alignment(GTK_MISC(urlLabel), 0.0, 0.0);
		gtk_box_pack_start(GTK_BOX(infoVbox), urlLabel, FALSE, FALSE, 0);
	}

	GtkWidget* icon = this->GetApplicationIcon();
	GtkWidget* infoBox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(infoBox), 5);
	gtk_box_pack_start(GTK_BOX(infoBox), icon, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(infoBox), infoVbox, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), infoBox, FALSE, FALSE, 0);

	GtkWidget* hseperator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), hseperator, FALSE, FALSE, 0);
}

void Installer::CreateIntroView()
{
	int width, height;
	std::string licenseText = this->app->GetLicenseText();
	if (licenseText.empty())
	{
		width = NO_LICENSE_WINDOW_WIDTH;
		height = NO_LICENSE_WINDOW_HEIGHT;
	}
	else
	{
		width = LICENSE_WINDOW_WIDTH;
		height = LICENSE_WINDOW_HEIGHT;
	}
	this->ResizeWindow(width, height);

	GtkWidget* windowVbox = gtk_vbox_new(FALSE, 0);
	this->CreateInfoBox(windowVbox);

	// Create the part with the license
	if (!licenseText.empty())
	{
		GtkWidget* licenseTextView = gtk_text_view_new();
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(licenseTextView), GTK_WRAP_WORD);
		gtk_text_buffer_set_text(
			gtk_text_view_get_buffer(GTK_TEXT_VIEW(licenseTextView)),
			licenseText.c_str(), -1);
		GtkWidget* scroller = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy (
			GTK_SCROLLED_WINDOW(scroller),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_add_with_viewport(
			GTK_SCROLLED_WINDOW(scroller), licenseTextView);

		gtk_container_set_border_width(GTK_CONTAINER(scroller), 5);
		gtk_box_pack_start(GTK_BOX(windowVbox), scroller, TRUE, TRUE, 0);
	}

	if (this->jobs.size() > 0)
	{
		GtkWidget* hseperator = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(windowVbox), hseperator, FALSE, FALSE, 0);

		///* Install dialog label */
		GtkWidget* label = gtk_label_new(
			"This application may need to download and install "
			"additional components. Where should they be installed?");
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_widget_set_size_request(label, width - 10, -1);

		/* Install type combobox */
		GtkListStore* store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
		GtkTreeIter iter;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
			0, GTK_STOCK_HOME,
			1, "Install to my home directory", -1);
		std::string text = std::string("Install to ") + systemRuntimeHome;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
			0, GTK_STOCK_DIALOG_AUTHENTICATION,
			1, text.c_str(), -1);
		this->installCombo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));

		GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(installCombo), renderer, FALSE);
		gtk_cell_layout_set_attributes(
			GTK_CELL_LAYOUT(installCombo), renderer,
			"stock-id", 0, NULL);
		renderer = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(installCombo), renderer, TRUE);
		gtk_cell_layout_set_attributes(
			GTK_CELL_LAYOUT(installCombo), renderer,
			"text", 1, NULL);
		gtk_combo_box_set_active(GTK_COMBO_BOX(installCombo), 0);

		/* Pack label and combobox into vbox */
		GtkWidget* installTypeBox = gtk_vbox_new(FALSE, 0);
		gtk_container_set_border_width(GTK_CONTAINER(installTypeBox), 5);
		gtk_box_pack_start(GTK_BOX(installTypeBox), label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(installTypeBox), installCombo, FALSE, FALSE, 10);
		gtk_box_pack_start(GTK_BOX(windowVbox), installTypeBox, FALSE, FALSE, 0);
	}

	// Add the buttons
	string continueText = "Install";
	if (this->jobs.size() == 0)
		continueText = "OK";

	GtkWidget* cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	GtkWidget* install = gtk_button_new_with_label(continueText.c_str());
	GtkWidget* install_icon = gtk_image_new_from_stock(
		GTK_STOCK_OK,
		GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(install), install_icon);
	GtkWidget* buttonBox = gtk_hbutton_box_new();
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(buttonBox), 5);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_END);
	gtk_box_pack_start(GTK_BOX(buttonBox), cancel, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(buttonBox), install, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(windowVbox), buttonBox, FALSE, FALSE, 0);

	g_signal_connect (
		G_OBJECT(cancel),
		"clicked",
		G_CALLBACK(cancel_cb),
		(gpointer) this);

	g_signal_connect (
		G_OBJECT(install),
		"clicked",
		G_CALLBACK(install_cb),
		(gpointer) this);

	gtk_container_add(GTK_CONTAINER(this->window), windowVbox);
	gtk_widget_show_all(this->window);

}

void Installer::CreateProgressView()
{
	// Remove all children from the window
	GList* children = gtk_container_get_children(GTK_CONTAINER(this->window));
	for (size_t i = 0; i < g_list_length(children); i++)
	{
		GtkWidget* w = (GtkWidget*) g_list_nth_data(children, i);
		gtk_container_remove(GTK_CONTAINER(this->window), w);
	}
	gtk_container_set_border_width(GTK_CONTAINER(this->window), 5);

	this->ResizeWindow(PROGRESS_WINDOW_WIDTH, PROGRESS_WINDOW_HEIGHT);
	GtkWidget* vbox = gtk_vbox_new(FALSE, 5);
	this->CreateInfoBox(vbox);

	this->downloadingLabel = gtk_label_new("Downloading packages...");
	gtk_box_pack_start(GTK_BOX(vbox), this->downloadingLabel, FALSE, FALSE, 2);

	//GtkWidget* icon = this->GetTitaniumIcon();
	//GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
	//gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 5);
	//gtk_box_pack_start(GTK_BOX(hbox), this->downloadingLabel, FALSE, FALSE, 0);

	this->progressBar = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(vbox), this->progressBar, FALSE, FALSE, 0);

	GtkWidget* hbox2 = gtk_hbox_new(FALSE, 0);
	GtkWidget* cancel_but = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_box_pack_start(GTK_BOX(hbox2), cancel_but, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 10);

	gtk_container_add(GTK_CONTAINER(this->window), vbox);

	g_signal_connect (
		G_OBJECT(cancel_but),
		"clicked",
		G_CALLBACK(cancel_cb),
		(gpointer) this);

	gtk_widget_show_all(this->window);
}

GtkWidget* Installer::GetTitaniumIcon()
{
	GdkColormap* colormap = gtk_widget_get_colormap(this->window);
	GdkBitmap *mask = NULL;
	GdkPixmap* pixmap = gdk_pixmap_colormap_create_from_xpm_d(
		NULL,
		colormap,
		&mask,
		NULL,
		(gchar**) titanium_xpm);
	return gtk_image_new_from_pixmap(pixmap, mask);

}

GtkWidget* Installer::GetApplicationIcon()
{
	GtkWidget* icon;
	if (this->app->image != "")
	{
		icon = gtk_image_new_from_file(this->app->image.c_str());
	}
	else // Use default Titanium icon
	{
		return this->GetTitaniumIcon();
	}
	return icon;
}

void Installer::StartInstallProcess()
{
	int choice = gtk_combo_box_get_active(GTK_COMBO_BOX(this->installCombo));
	if (choice == 0) // home directory install
	{
		this->StartDownloading();
	}
	else
	{
		this->SetStage(SUDO_REQUEST);
		gtk_widget_hide(this->window);
		gtk_main_quit();
	}

}

void Installer::StartDownloading()
{
	if (this->jobs.size() <= 0)
	{
		this->SetStage(SUCCESS);
		gtk_main_quit();
		return;
	}
	else
	{
		temporaryDirectory = FileUtils::GetTempDirectory();
		Job::download_dir = temporaryDirectory;
		Job::InitDownloader();

		this->CreateProgressView();
		this->SetStage(DOWNLOADING);

		g_thread_init(NULL);
		this->download_thread =
			g_thread_create(&download_thread_f, this, TRUE, NULL);

		if (this->download_thread == NULL)
			g_warning("Can't create download thread!\n");
	}

}

void Installer::UpdateProgress()
{
	Job *j = this->CurrentJob();

	if (this->window == NULL)
		return;

	Stage s = this->GetStage();
	if (j != NULL && (s == DOWNLOADING || s == INSTALLING))
	{
		double progress = j->GetProgress();
		gtk_progress_bar_set_fraction(
			GTK_PROGRESS_BAR(this->progressBar),
			progress);

		std::ostringstream text;
		if (s == INSTALLING)
			text << "Installing ";
		else
			text << "Downloading ";
		text << "package " << j->Index() << " of " << Job::total;

		gtk_label_set_text(GTK_LABEL(this->downloadingLabel), text.str().c_str());

	}
	else if (s == PREINSTALL)
	{
		gtk_progress_bar_set_fraction(
			GTK_PROGRESS_BAR(this->progressBar),
			1.0);
	}
	
}

static gboolean watcher(gpointer data)
{
	Installer::Stage s = Installer::instance->GetStage();
	if (s == Installer::PREINSTALL)
	{
		Installer::instance->StartInstalling();
	}
	else if (s == Installer::ERROR)
	{
		Installer::instance->ShowError();
		gtk_main_quit();
		return FALSE;
	}
	else if (s == Installer::CANCELLED)
	{
		gtk_main_quit();
		return FALSE;
	}
	else if (s == Installer::SUCCESS)
	{
		gtk_main_quit();
		return FALSE;
	}
	else
	{
		Installer::instance->UpdateProgress();
	}

	return TRUE;
}

void Installer::StartInstalling()
{
	if (this->download_thread != NULL)
	{
		g_thread_join(this->download_thread);
		this->download_thread = NULL;

		// This has to happen when no other threads are
		// running, since CURL's global shutdown is not
		// thread safe and we need access to downloaded
		// files now.
		Job::ShutdownDownloader();

		if(!g_thread_create(&install_thread_f, this, FALSE, NULL))
			g_warning("Can't create install thread!\n");

		this->SetStage(INSTALLING);
	}
}


void *download_thread_f(gpointer data)
{
	Installer* inst = (Installer*) data;
	std::vector<Job*>& jobs = inst->GetJobs();
	try
	{
		for (size_t i = 0; i < jobs.size(); i++)
		{
			Job *j = jobs.at(i);
			inst->SetCurrentJob(j);
			j->Fetch();

			// Wait for an unzip job to finish before actually cancelling
			if (Installer::instance->GetStage() == Installer::CANCEL_REQUEST)
			{
				Installer::instance->SetStage(Installer::CANCELLED);
				return NULL;
			}
		}
	}
	catch (std::exception& e)
	{
		std::string message = e.what();
		inst->SetError(message);
		return NULL;
	}
	catch (std::string& e)
	{
		inst->SetError(e);
		return NULL;
	}
	catch (...)
	{
		std::string message = "Unknown error";
		inst->SetError(message);
		return NULL;
	}

	inst->SetStage(Installer::PREINSTALL);
	return NULL;
}

void *install_thread_f(gpointer data)
{
	Installer* inst = (Installer*) data;
	std::vector<Job*>& jobs = Installer::instance->GetJobs();
	try
	{
		for (size_t i = 0; i < jobs.size(); i++)
		{
			Job *j = jobs.at(i);
			inst->SetCurrentJob(j);
			j->Unzip();

			// Wait for an unzip job to finish before actually cancelling
			if (inst->GetStage() == Installer::CANCEL_REQUEST)
			{
				inst->SetStage(Installer::CANCELLED);
				return NULL;
			}
		}
	}
	catch (std::exception& e)
	{
		std::string message = e.what();
		inst->SetError(message);
		return NULL;
	}
	catch (std::string& e)
	{
		inst->SetError(e);
		return NULL;
	}
	catch (...)
	{
		std::string message = "Unknown error";
		inst->SetError(message);
		return NULL;
	}

	inst->SetStage(Installer::SUCCESS);
	return NULL;
}

void Installer::ShowError()
{
	if (this->error != "")
	{
		GtkWidget* dialog = gtk_message_dialog_new(
			GTK_WINDOW(this->window),
			GTK_DIALOG_MODAL,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"%s",
			this->error.c_str());
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}
}

static void install_cb(GtkWidget *widget, gpointer data)
{
	Installer::instance->StartInstallProcess();
}

static void cancel_cb(GtkWidget *widget, gpointer data)
{
	Installer::Stage s = Installer::instance->GetStage();
	if (s == Installer::PREDOWNLOAD)
	{
		gtk_main_quit();
		Installer::instance->SetStage(Installer::CANCELLED);
	}
	else
	{
		// Wait for the download or install thread to cancel us
		Installer::instance->SetStage(Installer::CANCEL_REQUEST);
	}
}

static void destroy_cb(GtkWidget *widget, gpointer data)
{
	Installer::Stage s = Installer::instance->GetStage();
	if (s == Installer::PREDOWNLOAD)
	{
		gtk_main_quit();
		Installer::instance->SetStage(Installer::CANCELLED);
	}
	else
	{
		// Wait for the download or install thread to cancel us
		Installer::instance->SetStage(Installer::CANCEL_REQUEST);
	}
	Installer::instance->SetWindow(NULL);
}

// TODO: Switch to PolicyKit
int do_install_sudo()
{
	// Copy all but the first command-line arg
	std::vector<std::string> args;
	args.push_back("--description");
	args.push_back("Titanium Network Installer");

	args.push_back("--");
	args.push_back(argv[0]);
	args.push_back("--system");
	for (int i = 2; i < argc; i++)
	{
		args.push_back(argv[i]);
	}

	// Restart in a sudoed environment
	std::string cmd = "gksudo";
	int r = kroll::FileUtils::RunAndWait(cmd, args);
	if (r == 127)
	{
		// Erase gksudo specific options
		args.erase(args.begin());
		args.erase(args.begin());
		cmd = std::string("kdesudo");
		args.insert(args.begin(), "The Titanium network installer needs adminstrator privileges to run. Please enter your password.");
		args.insert(args.begin(), "--comment");
		args.insert(args.begin(), "-d");
		r = kroll::FileUtils::RunAndWait(cmd, args);
	}
	if (r == 127)
	{
		// Erase kdesudo specific option
		args.erase(args.begin());
		args.erase(args.begin());
		args.erase(args.begin());
		cmd = std::string("xterm");
		args.insert(args.begin(), "sudo");
		args.insert(args.begin(), "-e");
		r = kroll::FileUtils::RunAndWait(cmd, args);
	}
	return r;
}

int main(int _argc, const char* _argv[])
{
	argc = _argc;
	argv = (char **) _argv;
	gtk_init(&argc, &argv);

	string typeString = argv[1];
	string applicationPath = argv[2];
	userRuntimeHome = argv[3];
	systemRuntimeHome = argv[4];

	int installType;
	if (typeString == "--system")
	{
		installType = SYSTEM_INSTALL;
		Job::install_dir = systemRuntimeHome;
	}
	else
	{
		installType = HOMEDIR_INSTALL;
		Job::install_dir = userRuntimeHome;
	}

	vector<Job*> jobs;
	for (int i = 5; i < argc; i++)
	{
		jobs.push_back(new Job(argv[i]));
	}

	curl_global_init(CURL_GLOBAL_ALL);

	new Installer(applicationPath, jobs, installType);
	int result = Installer::instance->GetStage();

	if (result == Installer::SUDO_REQUEST)
	{
		result = do_install_sudo();
		Installer::instance->SetStage((Installer::Stage) result);
	}

	Installer::instance->FinishInstall();
	result = Installer::instance->GetStage();

	delete Installer::instance;

	Job::ShutdownDownloader();
	if (!temporaryDirectory.empty() && FileUtils::IsDirectory(temporaryDirectory))
		FileUtils::DeleteDirectory(temporaryDirectory); // Clean up

	return result;
	//return choose_install_path(argc, argv);
}
