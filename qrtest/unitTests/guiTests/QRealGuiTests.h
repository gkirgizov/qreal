/* Copyright 2015 QReal Research Group
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#pragma once

#include <gtest/gtest.h>
#include "qrgui/plugins/toolPluginInterface/usedInterfaces/mainWindowScriptAPIInterface.h"
#include "./../../../../qreal/qrgui/mainWindow/mainWindow.h"
#include "workaroundTestFunctions.h"
#include "TestAgent.h"

namespace guiTesting {

class TestAgent;

/// Test suite for standard qReal GUI
/// @warning Test suite can work incorrectly in hidden mode (e.g. scripts are running during ur work in Qt SDK)
/// @warning Segmantation fault in a single test crushes all tests
/// @warning Dont forget write reachedEndOfScript(); before quit() if u wanna quit application by himself
class QRealGuiTests : public testing::Test
{
protected:
	/// SetUp and TearDown are calling before/after every test
	void SetUp() override;
	void TearDown() override;

	/// start evaluating \a script
	/// @note Should be invoked once for every TEST
	void run(const QString &script);

	/// function for includes of needed files with common script
	/// @note u can include several needed files
	void includeCommonScript(const QString &relativeFileName);

	/// abort evaluating and close the program with the freeze code
	void failTest();

	/// @returns folder name with .js scripts for the test suite
	QString getScriptFolderName() const;
	/// set up folder name in \a folder. Just the folder name (without the other path)
	void setScriptFolderName(const QString &folder);

	/// @returns upper time limit for main window exposing, openning
	int getTimeToExpose() const;
	/// set up time limit for main window exposing, openning
	void setTimeToExpose(const int timeToExpose);

	/// @returns upper time limit for every test
	int getTimeLimit() const;
	/// set up timeLimit
	void setTimeLimit(const int timeLimit);

private:
	void checkScriptSyntax(const QString &script, const QString &errorMsg);
	void checkLastEvaluating(const QString &errorMsg);
	QString readFile(const QString &relativeFileName);
	/// @todo solve this problem using QScriptEngineAgent
	/// this function is a WORKAROUND
	void prepareScriptForRunning(QString &script);
	void exterminate(const int returnCode);

	QString mScriptFolderName = "qrealScripts";
	int mTimeToExpose = 7000;
	int mTimeLimit = 67000;

	qReal::MainWindow* mWindow;
	qReal::gui::MainWidnowScriptAPIInterface* mMainWindowScriptAPIInterface;
	int mReturnCode;

	/// for debugging and statistics
	QStringList mCurrentIncludedFiles;
	QString mCurrentTotalProgram;
	QString mCurrentEvaluatingScript;
	QString mCurrentEvaluatingScriptFile;

	/// for QTimer connects
	QString mScript;
	QString mFileName;
	QString mRelativeName;

	/// uses for debugging and scripting
	/// Doesn't have ownership
	TestAgent *mTestAgent;
};

}
