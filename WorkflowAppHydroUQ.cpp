/* *****************************************************************************
Copyright (c) 2016-2017, The Regents of the University of California (Regents).
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.

REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED HEREUNDER IS 
PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, 
UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

*************************************************************************** */

// Written: fmckenna

#include "WorkflowAppHydroUQ.h"
#include <MainWindowWorkflowApp.h>

#include <QPushButton>
#include <QScrollArea>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QLabel>
#include <QDebug>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <HydroEventSelection.h>
#include <RunLocalWidget.h>
#include <QProcess>
#include <QCoreApplication>
#include <RemoteService.h>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QHostInfo>
#include <QUuid>
#include <QSvgWidget>
#include <QFrame>
#include <QVBoxLayout>

#include <SimCenterComponentSelection.h>
#include "GeneralInformationWidget.h"
#include <SIM_Selection.h>
#include <RandomVariablesContainer.h>
#include <FEA_Selection.h>
#include <QDir>
#include <QFile>
#include <UQ_EngineSelection.h>
#include <LocalApplication.h>
#include <RemoteApplication.h>
#include <RemoteJobManager.h>
#include <RunWidget.h>
#include <InputWidgetBIM.h>
#include <UQ_EngineSelection.h>
#include <UQ_Results.h>

#include <EDP_Selection.h>

#include "CustomizedItemModel.h"

#include <QSettings>
#include <QUuid>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QHostInfo>
#include <GoogleAnalytics.h>

#include <Utils/ProgramOutputDialog.h>
#include <Utils/RelativePathResolver.h>
#include <SC_ToolDialog.h>
#include <SC_RemoteAppTool.h>
#include <QList>
#include <RemoteAppTest.h>
#include <QMenuBar>

// For Tools
// #include <EmptyDomainCFD/EmptyDomainCFD.h>
#include <GeoClawOpenFOAM/GeoClawOpenFOAM.h>
#include <WaveDigitalFlume/WaveDigitalFlume.h>
#include <coupledDigitalTwin/CoupledDigitalTwin.h>
#include <mpm/MPM.h>
// static pointer for global procedure set in constructor
static WorkflowAppHydroUQ *theApp = 0;

// global procedure
int getNumParallelTasks() {
    return theApp->getMaxNumParallelTasks();
}

WorkflowAppHydroUQ::WorkflowAppHydroUQ(RemoteService *theService, QWidget *parent)
    : WorkflowAppWidget(theService, parent)
{
    // set static pointer for global procedure
    theApp = this;

    //
    // create the various widgets
    //
    theRVs = RandomVariablesContainer::getInstance();
    theGI = GeneralInformationWidget::getInstance();
    theSIM = new SIM_Selection(true, true);
    theEventSelection = new HydroEventSelection(theRVs, theGI, this);
    theAnalysisSelection = new FEA_Selection(true);
    theUQ_Selection = new UQ_EngineSelection(ForwardReliabilitySensitivity);
    theEDP_Selection = new EDP_Selection(theRVs);
    theResults = theUQ_Selection->getResults();
    //theResults = new DakotaResultsSampling(theRVs);

    localApp = new LocalApplication("sWHALE.py");
    remoteApp = new RemoteApplication("sWHALE.py", theService);
    // localApp = new LocalApplication("Hydro-UQ workflow.py");
    // remoteApp = new RemoteApplication("Hydro-UQ workflow.py", theService);
    // localApp = new LocalApplication("Hydro-UQ.py");
    // remoteApp = new RemoteApplication("Hydro-UQ.py", theService);

    //QStringList filesToDownload; filesToDownload << "inputRWHALE.json" << "input_data.zip" << "Results.zip";
    theJobManager = new RemoteJobManager(theService);

    SimCenterWidget *theWidgets[1];// =0;
    theRunWidget = new RunWidget(localApp, remoteApp, theWidgets, 0);

    //
    // connect signals and slots
    //
    // error messages and signals
    connect(localApp,SIGNAL(setupForRun(QString &,QString &)), this, SLOT(setUpForApplicationRun(QString &,QString &)));
    connect(this,SIGNAL(setUpForApplicationRunDone(QString&, QString &)), theRunWidget, SLOT(setupForRunApplicationDone(QString&, QString &)));
    connect(localApp,SIGNAL(processResults(QString&)), this, SLOT(processResults(QString&)));
    connect(theJobManager,SIGNAL(processResults(QString&)), this, SLOT(processResults(QString&)));
    connect(remoteApp,SIGNAL(setupForRun(QString &,QString &)), this, SLOT(setUpForApplicationRun(QString &,QString &)));
    connect(theJobManager,SIGNAL(loadFile(QString&)), this, SLOT(loadFile(QString&)));

    connect(remoteApp,SIGNAL(successfullJobStart()), theRunWidget, SLOT(hide()));
       
    //connect(theRunLocalWidget, SIGNAL(runButtonPressed(QString, QString)), this, SLOT(runLocal(QString, QString)));

    // // From WE-UQ, should probably implement
    // connect(localApp,SIGNAL(runComplete()), this, SLOT(runComplete()));
    // connect(remoteApp,SIGNAL(successfullJobStart()), this, SLOT(runComplete()));
    // connect(theService, SIGNAL(closeDialog()), this, SLOT(runComplete()));
    // connect(theJobManager, SIGNAL(closeDialog()), this, SLOT(runComplete()));   


    //
    // create layout to hold component selectio
    //


    QHBoxLayout *horizontalLayout = new QHBoxLayout();
    this->setLayout(horizontalLayout);
    this->setContentsMargins(0,5,0,5);
    horizontalLayout->setMargin(0);
    horizontalLayout->setSpacing(0);

    theSvgUQ  = new QSvgWidget();
    theSvgGI  = new QSvgWidget();
    theSvgSIM = new QSvgWidget();
    theSvgEVT = new QSvgWidget();
    theSvgFEM = new QSvgWidget();
    theSvgEDP = new QSvgWidget();
    theSvgRV  = new QSvgWidget();
    theSvgRES = new QSvgWidget();
    theSvgUQ->load(QString(":/icons/question-dimensions-white.svg"));
    theSvgGI->load(QString(":/icons/building-white.svg"));
    theSvgSIM->load(QString(":/icons/shape-3-white.svg"));
    theSvgEVT->load(QString(":/icons/ripple-white.svg"));
    theSvgFEM->load(QString(":/icons/vector-triangle-white.svg"));
    theSvgEDP->load(QString(":/icons/chart-arrows-white.svg"));
    theSvgRV->load(QString(":/icons/dice-6-white.svg"));
    theSvgRES->load(QString(":/icons/flag-white.svg"));
    // Set size of SVG to match the size of the test in the side bar at theComponentSelection, e.g. "UQ"
    
    int iconSize = 36;
    theSvgUQ->setFixedSize(iconSize,iconSize);
    theSvgGI->setFixedSize(iconSize,iconSize);
    theSvgSIM->setFixedSize(iconSize,iconSize);
    theSvgEVT->setFixedSize(iconSize,iconSize);
    theSvgFEM->setFixedSize(iconSize,iconSize);
    theSvgEDP->setFixedSize(iconSize,iconSize);
    theSvgRV->setFixedSize(iconSize,iconSize);
    theSvgRES->setFixedSize(iconSize,iconSize);

    // Set background color of SVG to match the background color of the side bar
    // theSvgUQ->setStyleSheet("background-color: rgb(79, 83, 89)");
    // Set the size policy of the SVG to be fixed
    theSvgUQ->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    theSvgGI->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    theSvgSIM->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    theSvgEVT->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    theSvgFEM->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    theSvgEDP->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    theSvgRV->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    theSvgRES->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Create a layout to hold the icons
    QVBoxLayout *verticalIconLayout = new QVBoxLayout();
    verticalIconLayout->addWidget(theSvgUQ);
    verticalIconLayout->addWidget(theSvgGI);
    verticalIconLayout->addWidget(theSvgSIM);
    verticalIconLayout->addWidget(theSvgEVT);
    verticalIconLayout->addWidget(theSvgFEM);
    verticalIconLayout->addWidget(theSvgEDP);
    verticalIconLayout->addWidget(theSvgRV);
    verticalIconLayout->addWidget(theSvgRES);
    verticalIconLayout->addStretch();
    verticalIconLayout->setAlignment(Qt::AlignCenter);

    // Create a frame to hold the icons
    sideBarIconFrame = new QFrame();
    sideBarIconFrame->setLayout(verticalIconLayout);
    sideBarIconFrame->setFrameShape(QFrame::Box);
    sideBarIconFrame->setLineWidth(0);
    sideBarIconFrame->setObjectName("sideBarIconFrame");
    sideBarIconFrame->setStyleSheet("#sideBarIconFrame {background-color: rgb(63, 67, 73); border: 0px solid rgb(61, 65, 71); }");
    sideBarIconFrame->setContentsMargins(0,5,0,5);
    sideBarIconFrame->layout()->setContentsMargins(0,5,0,5);
    sideBarIconFrame->layout()->setSpacing(14);

    // Make sideBarIconFrame match the style of theComponentSelection
    sideBarIconFrame->setFixedWidth(50);
    sideBarIconFrame->setMinimumHeight(600);
    sideBarIconFrame->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    // Create a layout to hold the frame
    QVBoxLayout *wrapperVerticalIconLayout = new QVBoxLayout();
    wrapperVerticalIconLayout->addWidget(sideBarIconFrame);

    
    QFrame *wrapperFrame = new QFrame();
    wrapperFrame->setLayout(wrapperVerticalIconLayout);
    wrapperFrame->setFrameShape(QFrame::NoFrame);
    wrapperFrame->setLineWidth(0);
    wrapperFrame->setContentsMargins(0,2.975,0,2.975);
    wrapperFrame->layout()->setContentsMargins(0,2.975,0,2.975);

    wrapperFrame->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    // Now, place the frame in the primary horizontal layout (before the component selection text)
    horizontalLayout->addWidget(wrapperFrame);

    //
    // create the component selection & add the components to it
    //

    theComponentSelection = new SimCenterComponentSelection();
    horizontalLayout->addWidget(theComponentSelection);
    theComponentSelection->addComponent(QString("UQ"),  theUQ_Selection);
    theComponentSelection->addComponent(QString("GI"),  theGI);
    theComponentSelection->addComponent(QString("SIM"), theSIM);
    theComponentSelection->addComponent(QString("EVT"), theEventSelection);
    theComponentSelection->addComponent(QString("FEM"), theAnalysisSelection);
    theComponentSelection->addComponent(QString("EDP"), theEDP_Selection);
    theComponentSelection->addComponent(QString("RV"),  theRVs);
    theComponentSelection->addComponent(QString("RES"), theResults);
    theComponentSelection->displayComponent("EVT"); // Initial page on startup
    horizontalLayout->setAlignment(Qt::AlignLeft);

    // When theComponentSelection is changed, update the icon in the side bar to also be selected
    // connect(theComponentSelection, SIGNAL(selectionChanged(QString &)), this, SLOT(updateIcons(QString &)));

    // access a web page which will increment the usage count for this tool
    manager = new QNetworkAccessManager(this);
    connect(manager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(replyFinished(QNetworkReply*)));
    // TODO: Is this still functional? Check...
    manager->get(QNetworkRequest(QUrl("http://opensees.berkeley.edu/OpenSees/developer/hydrouq/use.php")));

    //
    // set the defults in the General Info
    //

    theGI->setDefaultProperties(1,144,360,360,37.8715,-122.2730);

    ProgramOutputDialog *theDialog=ProgramOutputDialog::getInstance();
    theDialog->appendInfoMessage("Welcome to HydroUQ");    
}

// Imported from WE-UQ for EmptyDomainCFD Tool
void
WorkflowAppHydroUQ::setMainWindow(MainWindowWorkflowApp* window) {

  this->WorkflowAppWidget::setMainWindow(window);

  auto menuBar = theMainWindow->menuBar();

  //
  // Add a Tool option to menu bar & add options to it
  //
  QMenu *toolsMenu = new QMenu(tr("&Tools"),menuBar);
  SC_ToolDialog *theToolDialog = new SC_ToolDialog(this);

  //
  // Add empty domain to Tools
  //
  // EmptyDomainCFD *theEmptyDomain = new EmptyDomainCFD(theRVs);
  MPM *theEmptyDomain = new MPM(theRVs); // TODO: Make an MPM EmptyDomainCFD class
  QString appName = "ClaymoreUW.bonusj-1.0.0"; // Frontera
  QList<QString> queues; queues << "rtx-dev" << "rtx";
  SC_RemoteAppTool *theEmptyDomainTool = new SC_RemoteAppTool(appName, queues, theRemoteService, theEmptyDomain, theToolDialog);
  theToolDialog->addTool(theEmptyDomainTool, "Digital Twin (MPM)");

  // Set the path to the input file
  QAction *showEmptyDomain = toolsMenu->addAction("&Digital Twin (MPM)");
  connect(showEmptyDomain, &QAction::triggered, this,[this, theDialog=theToolDialog, theEmp = theEmptyDomain] {
    theDialog->showTool("Digital Twin (MPM)");
    if (!theEmp->isInitialize()) {
	    theEmp->initialize();
    }
  });

  
  //
  // Add SimpleTest Example
  //


//   RemoteAppTest *theTest = new RemoteAppTest();
  CoupledDigitalTwin *theTest = new CoupledDigitalTwin(theRVs);
  QString appNameTest = "simcenter-openfoam-frontera-1.0.0u3";
  QList<QString> queuesTest; queuesTest << "normal" << "fast";
  SC_RemoteAppTool *theTestTool = new SC_RemoteAppTool(appNameTest, queuesTest, theRemoteService, theTest, theToolDialog);
  theToolDialog->addTool(theTestTool, "Digital Twin (OpenFOAM + OpenSees)");
  
  // Set the path to the input file
  QAction *showTest = toolsMenu->addAction("&Build and Run MPI Program");
  connect(showTest, &QAction::triggered, this,[this, theDialog=theToolDialog, theEmp = theTestTool] {
    theDialog->showTool("Digital Twin (OpenFOAM + OpenSees)");
    // if (!theEmp->isInitialize()) {
    //     theEmp->initialize();
    // }
  });  


  //
  // Add Tools to menu bar
  //

  QAction* menuAfter = nullptr;
  foreach (QAction *action, menuBar->actions()) {
    // First check for an examples menu and if that does not exist put it before the help menu
    auto actionText = action->text();
    if(actionText.compare("&Examples") == 0)
      {
	menuAfter = action;
	break;
      }
    else if(actionText.compare("&Help") == 0)
      {
	menuAfter = action;
	break;
      }
  }
  menuBar->insertMenu(menuAfter, toolsMenu);    
}


WorkflowAppHydroUQ::~WorkflowAppHydroUQ()
{
    // hack to get around a sometimes occuring seg fault
    // as some classes in destructur remove RV from the RVCOntainer
    // which may already have been destructed .. so removing so no destructor called

  //    QWidget *newUQ = new QWidget();
  //    theComponentSelection->swapComponent("RV",newUQ);
}

// TODO: Make this work
void
WorkflowAppHydroUQ::updateIcons(QString &componentName)
{
    // Update the icon in the side bar to also be selected
    // It does this by changing the background color of the icon to match the background color of theComponentSelection
    // The background color of theComponentSelection is changed in SimCenterComponentSelection::displayComponent(QString componentName)
    // This is done by changing the stylesheet of theComponentSelection
    // The stylesheet is changed in SimCenterComponentSelection::displayComponent(QString componentName)
    // The stylesheet is changed to match the stylesheet of the side bar icon

    // First, reset the background color of all icons to the default
    // theSvgUQ->setStyleSheet(":hover{background-color: rgb(79, 83, 89)}");
    // theSvgGI->setStyleSheet(":hover{background-color: rgb(79, 83, 89)}");
    // theSvgSIM->setStyleSheet(":hover{background-color: rgb(79, 83, 89)}");
    // theSvgEVT->setStyleSheet(":hover{background-color: rgb(79, 83, 89)}");
    // theSvgFEM->setStyleSheet(":hover{background-color: rgb(79, 83, 89)}");
    // theSvgEDP->setStyleSheet(":hover{background-color: rgb(79, 83, 89)}");
    // theSvgRV->setStyleSheet(":hover{background-color: rgb(79, 83, 89)}");
    // theSvgRES->setStyleSheet(":hover{background-color: rgb(79, 83, 89)}");

    // print(componentName);
    // Then, set the background color of the icon that was selected to match the background color of theComponentSelection
    if (componentName == QString("UQ") || componentName ==  QString("UQ_Selection")) {
        // Set active background
        // theSvgUQ->setStyleSheet(":active{background-color: rgb(63, 147, 168)}");
        // theSvgUQ->setStyleSheet(":active{background-color: rgb(63, 147, 168)}");
        theSvgUQ->setStyleSheet(":inactive{background-color: rgb(63, 147, 168)}");
        //redraw
        theSvgUQ->update();

    }
    else if (componentName == QString("GI") || componentName ==  QString("GI_Selection")) {
        theSvgGI->setStyleSheet(":active{background-color: rgb(63, 147, 168)}");
        theSvgGI->setStyleSheet(":hover{background-color: rgb(69, 187, 217)}");
        theSvgGI->update();
    }
    else if (componentName ==  QString("SIM")) {
        theSvgSIM->setStyleSheet(":active{background-color: rgb(63, 147, 168)}");
        theSvgSIM->setStyleSheet(":hover{background-color: rgb(69, 187, 217)}");
    }
    else if (componentName ==  QString("EVT")) {
        theSvgEVT->setStyleSheet(":active{background-color: rgb(63, 147, 168)}");
        theSvgEVT->setStyleSheet(":hover{background-color: rgb(69, 187, 217)}");
    }
    else if (componentName == "FEM") {
        theSvgFEM->setStyleSheet(":active{background-color: rgb(63, 147, 168)}");
        theSvgFEM->setStyleSheet(":hover{background-color: rgb(69, 187, 217)}");
    }
    else if (componentName == "EDP") {
        theSvgEDP->setStyleSheet(":active{background-color: rgb(63, 147, 168)}");
        theSvgEDP->setStyleSheet(":hover{background-color: rgb(69, 187, 217)}");
    }
    else if (componentName == "RV") {
        theSvgRV->setStyleSheet(":active{background-color: rgb(63, 147, 168)}");
        theSvgRV->setStyleSheet(":hover{background-color: rgb(69, 187, 217)}");
    }
    else if (componentName == "RES") {
        theSvgRES->setStyleSheet(":active{background-color: rgb(63, 147, 168)}");
        theSvgRES->setStyleSheet(":hover{background-color: rgb(69, 187, 217)}");
    } else {
        // Do nothing
        qDebug() << "No match of Icon to Sidebar tab text.";
    }

    return;
}



void WorkflowAppHydroUQ::replyFinished(QNetworkReply *pReply)
{
    return;
}

bool WorkflowAppHydroUQ::canRunLocally()
{
    // TODO: Look into local run 
    // QMessageBox msgBox;
    // msgBox.setText("The current workflow cannot run locally, please run at DesignSafe instead.");
    // msgBox.exec();
    // return false;
    // From WE-UQ:
    QList<SimCenterAppWidget*> apps({theEventSelection, theEDP_Selection, theSIM});

    foreach(SimCenterAppWidget* app, apps)
    {
        if(!app->supportsLocalRun())
        {
            theRunWidget->close();
            QMessageBox msgBox;
            msgBox.setText("The current workflow cannot run locally, please run at DesignSafe instead.");
            msgBox.exec();
            return false;
        }
    }
    return true;
}


bool
WorkflowAppHydroUQ::outputToJSON(QJsonObject &jsonObjectTop) {

    //
    // get each of the main widgets to output themselves
    //
  
    bool result = true;
    QJsonObject apps;

    //
    // get each of the main widgets to output themselves to top 
    // and workflow widgets to output appData to apps
    //

    // theGI
    QJsonObject jsonObjGenInfo;
    result = theGI->outputToJSON(jsonObjGenInfo);
    if (result == false)
        return result;
    jsonObjectTop["GeneralInformation"] = jsonObjGenInfo;

    // theRVs
    result = theRVs->outputToJSON(jsonObjectTop);
    if (result == false)
        return result;

    // theEDP
    QJsonObject jsonObjectEDP;
    result = theEDP_Selection->outputToJSON(jsonObjectEDP);
    if (result == false)
        return result;
    jsonObjectTop["EDP"] = jsonObjectEDP;

    QJsonObject appsEDP;
    result = theEDP_Selection->outputAppDataToJSON(appsEDP);
    if (result == false)
        return result;
    apps["EDP"]=appsEDP;

    // theUQ
    result = theUQ_Selection->outputAppDataToJSON(apps);
    if (result == false)
        return result;
    
    result = theUQ_Selection->outputToJSON(jsonObjectTop);
    if (result == false)
        return result;

    // theSIM
    result = theSIM->outputAppDataToJSON(apps);
    if (result == false)
        return result;

    result = theSIM->outputToJSON(jsonObjectTop);
    if (result == false)
        return result;

    // theAnalysis
    result = theAnalysisSelection->outputAppDataToJSON(apps);
    if (result == false)
        return result;

    result = theAnalysisSelection->outputToJSON(jsonObjectTop);
    if (result == false)
        return result;

    // theEventSelection
    // NOTE: Events treated differently, due to array nature of objects
    result = theEventSelection->outputToJSON(jsonObjectTop);
    if (result == false)
        return result;

    result = theEventSelection->outputAppDataToJSON(apps);
    if (result == false)
        return result;

    result = theRunWidget->outputToJSON(jsonObjectTop);
    if (result == false)
        return result;

    // --------------------------------------------
    // Should this be deprecated? Its not in WE-UQ - but it is in the original HydroUQ, (JB)
    // theResults
    // sy - to save results
    result = theResults->outputToJSON(jsonObjectTop);
    if (result == false)
        return result;
    // --------------------------------------------

    jsonObjectTop["Applications"]=apps;

    QJsonObject defaultValues;
    defaultValues["workflowInput"]=QString("scInput.json");    
    defaultValues["filenameAIM"]=QString("AIM.json");
    defaultValues["filenameEVENT"] = QString("EVENT.json");
    defaultValues["filenameSAM"]= QString("SAM.json");
    defaultValues["filenameEDP"]= QString("EDP.json");
    defaultValues["filenameSIM"]= QString("SIM.json");
    defaultValues["driverFile"]= QString("driver");
    defaultValues["filenameDL"]= QString("BIM.json");
    defaultValues["workflowOutput"]= QString("EDP.json");
    QJsonArray rvFiles, edpFiles;
    rvFiles.append(QString("AIM.json"));
    rvFiles.append(QString("SAM.json"));
    rvFiles.append(QString("EVENT.json"));
    rvFiles.append(QString("SIM.json"));
    edpFiles.append(QString("EDP.json"));
    defaultValues["rvFiles"]= rvFiles;
    defaultValues["edpFiles"]=edpFiles;
    jsonObjectTop["DefaultValues"]=defaultValues;

    return result;
}


void WorkflowAppHydroUQ::processResults(QString &dirName)
{
  //
  // get results widget for currently selected UQ option
  //

  theResults = theUQ_Selection->getResults();
  if (theResults == NULL) {
    this->errorMessage("FATAL - UQ option selected not returning results widget");
    return;
  }

  //
  // connect signals for results widget
  //

  /*
  connect(theResults,SIGNAL(sendStatusMessage(QString)), this,SLOT(statusMessage(QString)));
  connect(theResults,SIGNAL(sendErrorMessage(QString)), this,SLOT(errorMessage(QString)));
  */
  
  //
  // swap current results with existing one in selection & disconnect signals
  //

  QWidget *oldResults = theComponentSelection->swapComponent(QString("RES"), theResults);
  if (oldResults != NULL && oldResults != theResults) {
    //disconnect(oldResults,SIGNAL(sendErrorMessage(QString)), this,SLOT(errorMessage(QString)));
    //disconnect(oldResults,SIGNAL(sendFatalMessage(QString)), this,SLOT(fatalMessage(QString)));  
    delete oldResults;
  }

  //
  // process results
  // 

  theResults->processResults(dirName);
//   theRunWidget->hide(); // Not in WE-UQ, so I removed it for now (JB)
  theComponentSelection->displayComponent("RES");
}

void
WorkflowAppHydroUQ::clear(void)
{
    // theRVs->clear(); // WE-UQ
    // theUQ_Selection->clear();  // WE-UQ
    theGI->clear();
    theSIM->clear();
    // theEventSelection->clear(); // WE-UQ
    // theAnalysisSelection->clear(); // WE-UQ
 
    // theResults=theUQ_Selection->getResults(); // WE-UQ
    // if (theResults == NULL) {
    //     this->errorMessage("FATAL - UQ option selected not returning results widget"); // WE-UQ
    //     return; // WE-UQ
    // }

    // abiy - added results processing and template stuff for test setup
    //
    // swap current results with existing one in selection & disconnect signals
    //

    // QWidget *oldResults = theComponentSelection->swapComponent(QString("RES"), theResults);
    
    // if (oldResults != NULL && oldResults != theResults) {
    //     delete oldResults;
    // }

    //
    // process results
    //
} 

bool
WorkflowAppHydroUQ::inputFromJSON(QJsonObject &jsonObject)
{
    //
    // get each of the main widgets to input themselves
    //

    if (jsonObject.contains("GeneralInformation")) {
        QJsonObject jsonObjGeneralInformation = jsonObject["GeneralInformation"].toObject();
        if (theGI->inputFromJSON(jsonObjGeneralInformation) == false) {
            this->errorMessage("_UQ: failed to read GeneralInformation");
        }
    } else {
        this->errorMessage("Hydro_UQ: failed to find GeneralInformation");
        return false;
    }

    if (jsonObject.contains("Applications")) {

        QJsonObject theApplicationObject = jsonObject["Applications"].toObject();

        // note: Events is different because the object is an Array
        if (theApplicationObject.contains("Events")) {
            //  QJsonObject theObject = theApplicationObject["Events"].toObject(); it is null object, actually an array
            if (theEventSelection->inputAppDataFromJSON(theApplicationObject) == false) {
                this->errorMessage("Hydro_UQ: failed to read Event Application");
            }

        } else {
            this->errorMessage("Hydro_UQ: failed to find Event Application");
            return false;
        }

        if (theUQ_Selection->inputAppDataFromJSON(theApplicationObject) == false)
            this->errorMessage("Hydro_UQ: failed to read UQ application");

        if (theSIM->inputAppDataFromJSON(theApplicationObject) == false)
            this->errorMessage("Hydro_UQ: failed to read SIM application");
    
        if (theAnalysisSelection->inputAppDataFromJSON(theApplicationObject) == false)
            this->errorMessage("Hydro_UQ: failed to read FEM application");

        if (theApplicationObject.contains("EDP")) {
            QJsonObject theObject = theApplicationObject["EDP"].toObject();
            if (theEDP_Selection->inputAppDataFromJSON(theObject) == false) {
                this->errorMessage("Hydro_UQ: failed to read EDP application");
            }
        } else {
            this->errorMessage("Hydro_UQ: failed to find EDP application");
            return false;
        }

    } else
        return false;

    /*
    ** Note to me - RVs and Events treated differently as both use arrays .. rethink API!
    */

    theEventSelection->inputFromJSON(jsonObject);
    theRVs->inputFromJSON(jsonObject);
    theRunWidget->inputFromJSON(jsonObject);

    if (jsonObject.contains("EDP")) {
        QJsonObject edpObj = jsonObject["EDP"].toObject();
        if (theEDP_Selection->inputFromJSON(edpObj) == false)
            this->errorMessage("Hydro_UQ: failed to read EDP data");
    } else {
        this->errorMessage("Hydro_UQ: failed to find EDP data");
        return false;
    }


    if (theUQ_Selection->inputFromJSON(jsonObject) == false)
       this->errorMessage("Hydro_UQ: failed to read UQ Method data");

    if (theAnalysisSelection->inputFromJSON(jsonObject) == false)
        this->errorMessage("Hydro_UQ: failed to read FEM Method data");

    if (theSIM->inputFromJSON(jsonObject) == false)
        this->errorMessage("Hydro_UQ: failed to read SIM Method data");

    // sy - to display results
    auto* theNewResults = theUQ_Selection->getResults();

    if (theNewResults->inputFromJSON(jsonObject) == false)
        this->errorMessage("Hydro_UQ: failed to read RES Method data");
    theResults->setResultWidget(theNewResults);

    this->statusMessage("Done Loading File");
    
    return true;  
}


void
WorkflowAppHydroUQ::onRunButtonClicked() {
    emit errorMessage("The HydroUQ application cannot be run locally. Please run remotely on DesignSafe.");
//    theRunWidget->hide();
//    theRunWidget->setMinimumWidth(this->width()*0.5);
//    theRunWidget->showLocalApplication();
//    GoogleAnalytics::ReportLocalRun();
}

void
WorkflowAppHydroUQ::onRemoteRunButtonClicked(){
    emit errorMessage("");

    bool loggedIn = theRemoteService->isLoggedIn();

    if (loggedIn == true) {
        theRunWidget->hide();
        theRunWidget->setMinimumWidth(this->width()*0.5);
        theRunWidget->showRemoteApplication();

    } else {
        errorMessage("ERROR - You Need to Login to DesignSafe to Run HydroUQ Remotely.");
    }

    GoogleAnalytics::ReportDesignSafeRun();
}

void
WorkflowAppHydroUQ::onRemoteGetButtonClicked(){

    emit errorMessage("");

    bool loggedIn = theRemoteService->isLoggedIn();

    if (loggedIn == true) {

        theJobManager->hide();
        theJobManager->updateJobTable("");
        theJobManager->show();

    } else {
        errorMessage("ERROR - You Need to Login to DesignSafe to Get HydroUQ Results From Remote Storage.");
    }
}

void
WorkflowAppHydroUQ::onExitButtonClicked(){

}

void
WorkflowAppHydroUQ::setUpForApplicationRun(QString &workingDir, QString &subDir) {

    errorMessage("");

    //
    // create temporary directory in working dir
    // and copy all files needed to this directory by invoking copyFiles() on app widgets
    //

    // designsafe will need a unique name
    /* *********************************************
    will let ParallelApplication rename dir
    QUuid uniqueName = QUuid::createUuid();
    QString strUnique = uniqueName.toString();
    strUnique = strUnique.mid(1,36);
    QString tmpDirName = QString("tmp.SimCenter") + strUnique;
    *********************************************** */

    QString tmpDirName = QString("tmp.SimCenter");
    qDebug() << "TMP_DIR: " << tmpDirName;
    QDir workDir(workingDir);

    QString tmpDirectory = workDir.absoluteFilePath(tmpDirName);
    QDir destinationDirectory(tmpDirectory);

    if(destinationDirectory.exists()) {
      destinationDirectory.removeRecursively();
    } else
      destinationDirectory.mkpath(tmpDirectory);

    QString templateDirectory  = destinationDirectory.absoluteFilePath(subDir);
    destinationDirectory.mkpath(templateDirectory);

    // copyPath(path, tmpDirectory, false);
    theSIM->copyFiles(templateDirectory);
    theEventSelection->copyFiles(templateDirectory);
    theAnalysisSelection->copyFiles(templateDirectory);
    theUQ_Selection->copyFiles(templateDirectory);
    theEDP_Selection->copyFiles(templateDirectory);

    //
    // in new templatedir dir save the UI data into dakota.json file (same result as using saveAs)
    // NOTE: we append object workingDir to this which points to template dir
    //

    QString inputFile = templateDirectory + QDir::separator() + tr("scInput.json");

    QFile file(inputFile);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        //errorMessage();
        return;
    }
    QJsonObject json;
    if (this->outputToJSON(json) == false)
        return;

    json["runDir"]=tmpDirectory;
    json["WorkflowType"]="Building Simulation";


    QJsonDocument doc(json);
    file.write(doc.toJson());
    file.close();


    // This is in WE-UQ, maybe needed for some CFD...
    // QJsonArray events = json["Applications"].toObject()["Events"].toArray();

    // bool hasCFDEvent = false;
    // QJsonObject eventAppData;
    // for (QJsonValueRef eventJson: events)
    // {
    //     QString eventApp = eventJson.toObject()["Application"].toString();
    //     if(0 == eventApp.compare("CFDEvent", Qt::CaseSensitivity::CaseInsensitive))
    //     {
    //         eventAppData = eventJson.toObject()["ApplicationData"].toObject();
    //         hasCFDEvent = true;
    //         break;
    //     }
    // }

    // RemoteApplication* remoteApplication = static_cast<RemoteApplication*>(remoteApp);


    // if(currentApp == remoteApp)
    // {
    //     remoteApplication->clearExtraInputs();
    //     remoteApplication->clearExtraParameters();
    // }
    // else
    // {
    //     if (!canRunLocally())
    //         return;
    // }

    // if(hasCFDEvent)
    // {
    //     //Adding extra job inputs for CFD
    //     QMap<QString, QString> extraInputs;
    //     if(eventAppData.contains("OpenFOAMCase"))
    //         extraInputs.insert("OpenFOAMCase", eventAppData["OpenFOAMCase"].toString());
    //     remoteApplication->setExtraInputs(extraInputs);

    //     //Adding extra job parameters for CFD
    //     QMap<QString, QString> extraParameters;
    //     if(eventAppData.contains("OpenFOAMSolver"))
    //         extraParameters.insert("OpenFOAMSolver", eventAppData["OpenFOAMSolver"].toString());
    //     remoteApplication->setExtraParameters(extraParameters);
    // }
    //

    statusMessage("Set-Up Done .. Now Starting HydroUQ Application");
    emit setUpForApplicationRunDone(tmpDirectory, inputFile);
}

int
WorkflowAppHydroUQ::loadFile(QString &fileName){

    //
    // open file
    //

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage(QString("Could Not Open File: ") + fileName);
        return -1; 
    }

    //
    // place contents of file into json object
    //

    QString val;
    val=file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
    QJsonObject jsonObj = doc.object();

    // close file
    file.close();

    //Resolve absolute paths from relative ones
    QFileInfo fileInfo(fileName);
    SCUtils::ResolveAbsolutePaths(jsonObj, fileInfo.dir());
    
    //
    // clear current and input from new JSON
    //

    this->clear();
    bool result = this->inputFromJSON(jsonObj);
    
    if (result == false)
      return -1;
    else
      return 0;
}

int
WorkflowAppHydroUQ::getMaxNumParallelTasks() {
    return theUQ_Selection->getNumParallelTasks();
}
