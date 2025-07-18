#!/usr/bin/env python3
#** *****************************************************************************
# *
# * If not stated otherwise in this file or this component's LICENSE file the
# * following copyright and licenses apply:
# *
# * Copyright 2024 RDK Management
# *
# * Licensed under the Apache License, Version 2.0 (the "License");
# * you may not use this file except in compliance with the License.
# * You may obtain a copy of the License at
# *
# *
# http://www.apache.org/licenses/LICENSE-2.0
# *
# * Unless required by applicable law or agreed to in writing, software
# * distributed under the License is distributed on an "AS IS" BASIS,
# * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# * See the License for the specific language governing permissions and
# * limitations under the License.
# *
#* ******************************************************************************

import os
import sys
import time

dir_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(dir_path, "../"))

from raft.framework.plugins.ut_raft.configRead import ConfigRead
from raft.framework.core.logModule import logModule
from raft.framework.plugins.ut_raft import utHelperClass
from raft.framework.plugins.ut_raft.utSuiteNavigator import UTSuiteNavigatorClass
from raft.framework.plugins.ut_raft.utBaseUtils import utBaseUtils

class  AVModule_Tests(utHelperClass):
    """
    Test class to run the L1 and L2 tests on the device.

    This class uses the `tvSettingsClass` to interact with the device

    """
    def __init__(self, log:logModule=None):
        """
        Initializes the test class
        """
        testName = "AVModule"
        qcId = "1"
        super().__init__(testName, qcId, log)

        self.moduleName = testName
        deviceTestSetup = self.cpe.get("test")
        targetWorkspace = self.cpe.get("target_directory")
        prompt = self.cpe.get("prompt")

        # kept for the process restart between test cases if required
        self.vSinkprocessName = self.cpe.get("vsink_process_name")
        self.vDecoderprocessName = self.cpe.get("vdecoder_process_name")
        self.aSinkprocessName = self.cpe.get("asink_process_name")
        self.aDecoderprocessName = self.cpe.get("adecoder_process_name")
        self.processPath = self.cpe.get("process_path")
        self.IsAVBufferRestart = self.cpe.get("AVBuffer_Restart")

        # Get path to device profile file
        self.moduleConfigProfileFile = os.path.join(dir_path, deviceTestSetup.get("profile"))
        # Get the target workspace
        self.targetWorkspace = targetWorkspace

        # Load test setup configuration
        testSetupPath = os.path.join(dir_path, "AVModule_setup.yaml" )
        self.testConfigFile = "AVModule_testConfig.yaml"
        self.testSetup = ConfigRead(testSetupPath, self.moduleName)
        self.hal_session = self.dut.getConsoleSession("ssh_hal_test")
        self.hal_session.prompt = prompt
#        self.process_session = self.dut.getConsoleSession("ssh_process")
#        self.process_session.prompt = prompt

        self.binPath = "bin/" #bin path which is coming from the build 
        self.profilePath = "profiles/"

        # Load configurations for device profile and menu
        self.deviceProfile = ConfigRead( self.moduleConfigProfileFile , self.moduleName)
        self.testConfig    = ConfigRead(self.testConfigFile, self.moduleName)

        # Forming the run.sh exe path and tar copy path /tmp/<modulename>/bin/run.sh and /tmp/<modulename>
        targetWorkspace = os.path.join(targetWorkspace,self.moduleName)
        targetWorkspace = targetWorkspace.replace("\\", "/")
        runPath = os.path.join(targetWorkspace,self.binPath)
        runPath = runPath.replace("\\", "/")

        # Forming the profile path /tmp/<modulename>/bin/profiles/hfp-profile.yaml
        moduleConfigProfileFilePath = os.path.join(self.profilePath,os.path.basename(self.moduleConfigProfileFile))
        moduleConfigProfileFilePath = moduleConfigProfileFilePath.replace("\\", "/")

        #Forming exe command
        self.testConfig.test.execute = self.testConfig.test.execute + f" -p {moduleConfigProfileFilePath}"

        self.utMenu        = UTSuiteNavigatorClass(self.testConfig, None, self.hal_session)
        self.testSession   = self.hal_session
        self.utilities         = utBaseUtils()

        tarFilesPath = os.path.normpath(os.path.join(dir_path, *self.testConfig.test.artifacts))
        self.utilities.scpCopy(self.testSession, tarFilesPath, targetWorkspace)

        self.change_directory(self.testSession, targetWorkspace)
        self.untar(self.testSession, self.testConfig.test.artifacts, targetWorkspace)
        self.change_directory(self.testSession, runPath)

    def untar(self, session, tar_gz_path, extract_path):
        """
        Untar a .tar.gz file on the remote device via the SSH session.

        Args:
            session (session class): The active SSH session object.
            tar_gz_path (str): Full path of the .tar.gz file on the remote device.
            extract_path (str): Directory on the remote device to extract the files.

        Returns:
            bool: True if successful, False otherwise.
        """

        if session.type != "ssh":
            self.log.fatal("Session type must be 'ssh'")
            return False

        # If tar_gz_path is a list of strings
        if isinstance(tar_gz_path, list):
            tar_gz_path = os.path.normpath(os.path.join(*tar_gz_path))

        # Now safe to use basename
        tar_file_name = os.path.basename(tar_gz_path)

        # You can optionally change directory before untarring
        # e.g., cd to extract_path and untar from there if needed:
        # cmd_untar = f"cd {extract_path} && tar -xzf {tar_gz_path}"
        # But generally, -C {extract_path} is enough.

        # Untar command on the remote device (using full path of tar file)
        cmd_untar = f"tar -xzf {tar_file_name} -C {extract_path}"
        session.write(cmd_untar)

        # Wait for command to complete (adapt prompt accordingly)
        output = session.read_until(session.prompt, timeout=5)
        self.log.info(output)
        # Simple error check
        if "tar:" in output.lower() or "error" in output.lower():
            self.log.error(f"Failed to untar on remote: {output}")
            return False
        else:
            self.log.info(f"Successfully untarred {tar_file_name} on remote to {extract_path}")
            return True

    def change_directory(self, session, directory_path):
        """
        Changes the working directory on the remote device via SSH and verifies the change.

        Args:
            session (session class): The active SSH session object.
            directory_path (str): The path of the directory to change to on the remote device.

        Returns:
            bool: True if successfully changed, False otherwise.
        """
        if session.type != "ssh":
            self.log.fatal("Session type must be 'ssh'")
            return False

        # Combine cd and pwd to verify
        cd_cmd = f"cd {directory_path} && pwd"
        session.write(cd_cmd)
        self.log.info(f"Changing directory on remote to: {directory_path}")

        # Read output to verify current directory
        output = session.read(timeout=5)
        
        if directory_path.rstrip("/") in output:
            self.log.info(f"Successfully changed to directory: {directory_path}")
            return True
        else:
            self.log.error(f"Failed to change directory. Output:\n{output}")
            return False

    def restart_process_by_name(self, session, process_name, binary_dir="/usr/bin"):
        """
        Kills and restarts a process on the remote device via SSH.

        Args:
            session (session class): Active SSH session object.
            process_name (str): The name of the running process (and binary).
            binary_dir (str): Directory where the binary resides (default: /usr/bin)

        Returns:
            bool: True if successfully restarted, False otherwise.
        """
        if session.type != "ssh":
            return False

        binary_path = os.path.join(binary_dir, process_name)

        # Kill the process
        kill_cmd = f"pkill -f {binary_path}"
        session.write(kill_cmd)
        self.log.info(f"Killing process: {binary_path}")
        output = session.read_until(session.prompt, timeout=5)
        self.log.info(output)

        # Start the process
        start_cmd = f"{binary_path} &"
        session.write(start_cmd)
        self.log.info(f"Restarting process: {start_cmd}")
        output = session.read_until(session.prompt, timeout=5)
        self.log.info(output)

        # Verify it's running
        verify_cmd = f"ps -ef | grep -i {process_name} | grep -v grep"
        session.write(verify_cmd)
        output = session.read_until(session.prompt, timeout=5)
        self.log.info(output)

        if process_name in output:
            self.log.info(f"{process_name} restarted successfully.")
            return True
        else:
            self.log.error(f"Failed to restart {process_name}.")
            return False

    def runTest(self, testSuite:str="L3 AVModule", test_case:str=None):
        """
        Runs the test case passed to this funtion
        Args:
            test_case (str, optional): test case name to run, default runs all test
        Returns:
            bool: True - test pass, False - test fails
        """
        self.utMenu.start()
        output = self.utMenu.select(testSuite, test_case)
        self.utMenu.stop()
        results = self.utMenu.collect_results(output)
        if results == None:
            results = False
        return results

    def testFunction(self):
        """
        The main test function to run the L1 and L2 tests.

        Returns:
            bool: Final result of the test.
        """
        testsuites = self.testSetup.fields.get('test_suites')

        finalresult = True

        for testsuite in testsuites:
            testsuite_name = testsuite.get("name")

            test_cases = testsuite.get("test_cases")

            if len(test_cases) == 1 and test_cases[0] == "all":
                self.log.stepStart(f'Test Suit: {testsuite_name} Run all Tests cases')
                # If 'all' test case mentioned in list, run all tests with 'r' option
                result = self.runTest(testsuite)
                if not result:
                    finalresult = False
                self.log.stepResult(result, f'Test Suit: {testsuite_name} Run all Tests cases')
            else:
                for test_case in testsuite.get("test_cases"):
                    self.log.stepStart(f'Test Suit: {testsuite_name} Test Case: {test_case}')
                    if self.IsAVBufferRestart == 1:
                        result = self.restart_process_by_name(self.hal_session, "RDKAVBufferService", "/usr/bin/")
                        time.sleep(2)  # Small delay after AVBuffer restart

                    #Ensure processName and processPath are both not None
                    if self.vSinkprocessName and self.processPath:
                        result = self.restart_process_by_name(self.hal_session, self.vSinkprocessName, self.processPath)
                        time.sleep(2)
                    else:
                        self.log.info("VSink Process name or process path is None — skipping process restart.")

                    if self.vDecoderprocessName and self.processPath:
                        result = self.restart_process_by_name(self.hal_session, self.vDecoderprocessName, self.processPath)
                        time.sleep(2)
                    else:
                        self.log.info("VSink Process name or process path is None — skipping process restart.")

                    if self.aSinkprocessName and self.processPath:
                        result = self.restart_process_by_name(self.hal_session, self.aSinkprocessName, self.processPath)
                        time.sleep(2)
                    else:
                        self.log.info("VSink Process name or process path is None — skipping process restart.")

                    if self.aDecoderprocessName and self.processPath:
                        result = self.restart_process_by_name(self.hal_session, self.aDecoderprocessName, self.processPath)
                        time.sleep(2)
                    else:
                        self.log.info("VSink Process name or process path is None — skipping process restart.")


                    result = self.runTest(testsuite_name, test_case)
                    if not result:
                        finalresult = False
                    self.log.stepResult(result, f'Test Suit: {testsuite_name} Test Case: {test_case}')

        return finalresult

if __name__ == '__main__':
    summerLogName = os.path.splitext(os.path.basename(__file__))[0] + "_summery"
    summeryLog = logModule(summerLogName, level=logModule.INFO)
    test = AVModule_Tests(summeryLog)
    test.run(False)
