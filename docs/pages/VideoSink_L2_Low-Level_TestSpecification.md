# VIDEO SINK L2 Low Level Test Specification and Procedure Documentation

## Table of Contents
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Acronyms, Terms and Abbreviations](#acronyms-terms-and-abbreviations)
  - [Definitions](#definitions)
  - [References](#references)
  - [Level 2 Test Procedure](#level-2-test-procedure)

## Overview
This document describes the level 2 testing suite for the VIDEOSINK module.

### Acronyms, Terms and Abbreviations

- `HAL` - Hardware Abstraction Layer, may include some common components
- `UT`  - Unit Test(s)
- `OEM` - Original Equipment Manufacturer
- `SoC` - System on a Chip

### Definitions
`ut-core` \- Common Testing Framework [https://github.com/rdkcentral/ut-core](https://github.com/rdkcentral/ut-core), which wraps an open-source framework that can be expanded to the requirements for future frameworks.

### References
`High Level Test Specification` - [videoSink_High-Level_TestSpecification.md](videoSink_High-Level_TestSpecification.md)

## Level 2 Test Procedure
The following functions are expected to test the module operates correctly.

### Test 1

|Title|Details|
|-----|------ |
|Test Name|`testsetandgetproperty`|
| Description | <li>Read available Video Sinks using IVideoSinkManager class.</li><li> Loop through available video sinks.</li> <li> Use the setProperty method to set all the properties.</li><li> Get the property value using getProperty of IVideoSink and verify the value.</li>|
| Test Group | Module : L2|
| Test Case ID | 001|
| Priority |High |

**Pre-Conditions :**
None

**Dependencies :**
None

**User Interaction :**
If user chose to run the test in interactive mode, then the test case has to be selected via console.

#### Test Procedure :

|Variation / Steps |Description |Test Data |Expected Result |Notes |
|----------------- |------------|----------|----------------|------|
| 01 | Initialize the test by setting up the VTS_L2_VideoSink Class object |None | None | Should be successful |
| 02 | Get all available sink Ids using `getVideoSinkIds` of VideoSinkManager Object | None |List of Sink Ids|Should be successful|
| 03 | Loop through each Sink Ids and get the Video Sink objects using `getVideoSink`| Sink Ids| Video Sink Object |Should be successful |
| 04 | Open the Video sink using `open` | Video Sink Object, Listener Object, Controller Object|Status is OK, Listenre assigned with new object| Should be successful |
| 05 | For each video sink, set the property using `setProperty` method | property = AV_SOURCE, setValue = 2 | Status is OK, setResult is true | Should be successful |
| 06 | Verify the property value using `getProperty` method | property = AV_SOURCE | Status is OK, getPropertyValue is 2 | Should be successful |
| 07 | Set another property using `setProperty` method | property = RENDER_FIRST_FRAME, setValue = 1 | Status is OK, setResult is true | Should be successful |
| 08 | Verify the second property value using `getProperty` method |property = RENDER_FIRST_FRAME | Status is OK, getPropertyValue is 1 | Should be successful |
| 09 | Clean up by closing each controller and clearing the environment | None | None | Should be successful |

### Test 2

|Title|Details|
|-----|------ |
|Test Name|`TestSetAndGetProperty_multipleInstance`|
| Description | <li>Read available Video Sinks using IVideoSinkManager class.</li><li> Loop through available video sinks.</li><li>OPen All the Video sinks</li> <li> Use the setProperty method to set all the properties.</li><li> Get the property value using getProperty of IVideoSink and verify the value.</li>|
| Test Group | Module : L2|
| Test Case ID | 002|
| Priority |High |

**Pre-Conditions :**
None

**Dependencies :**
None

**User Interaction :**
If user chose to run the test in interactive mode, then the test case has to be selected via console.

#### Test Procedure :

|Variation / Steps |Description |Test Data |Expected Result |Notes |
|----------------- |------------|----------|----------------|------|
| 01 | Initialize the test by setting up the VTS_L2_VideoSink Class object |None | None | Should be successful |
| 02 | Get all available sink Ids using `getVideoSinkIds` of VideoSinkManager Object | None |List of Sink Ids|Should be successful|
| 03 | Loop through each Sink Ids and get the Video Sink objects using `getVideoSink`| Sink Ids| Video Sink Object |Should be successful |
| 04 | Open all the Video sinks using `open` | Video Sink Object, Listener Object, Controller Object|Status is OK, listener assigned with new object| Should be successful |
| 05 | For each video sink, set the property using `setProperty` method | property = AV_SOURCE, setValue = 2 | Status is OK, setResult is true | Should be successful |
| 06 | Verify the property value using `getProperty` method | property = AV_SOURCE | Status is OK, getPropertyValue is 2 | Should be successful |
| 07 | Set another property using `setProperty` method | property = RENDER_FIRST_FRAME, setValue = 1 | Status is OK, setResult is true | Should be successful |
| 08 | Verify the second property value using `getProperty` method |property = RENDER_FIRST_FRAME | Status is OK, getPropertyValue is 1 | Should be successful |
| 09 | Clean up by closing each controller and clearing the environment | None | None | Should be successful |

### Test 3

| Title |Details |
| ----- |------- |
| Test  Name | `TestStateTransitions` |
| Description | Read supported video sinks using IVideoSinkManager class calling getVideoSinkIds. Loop through supported video sinks, register event listeners using registerEventListener. Ensure state transitions (opening-> Ready-> Starting-> started -> Flushing-> stopping -> closing -> Closed). Call open in IVideoSinkController to transition to the state Ready. Observe the OnStateChangeEvent in the IVideoSinkEventListener to get OPENING and READY states. Once in READY state query the Sink state using getState and verify. Call start in IVideoSinkController to transition to the state STARTED. Observe the OnStateChangeEvent in the IVideoSinkEventListener to get STARTING and STARTED states. Once in STARTED state query the Sink state using getState and verify. Call flush in IVideoSinkController to go through transitioning state FLUSHING. Observe the OnStateChangeEvent in the IVideoSinkEventListener to get FLUSHING and STARTED states. Once in STARTED state query the Sink state using getState and verify. Call close in IVideoSinkController to transition to the state CLOSED. Observe the OnStateChangeEvent in the IVideoSinkEventListener to get STOPPING, CLOSING and CLOSED states. Once in CLOSED state query the Sink state using getState and verify. |
| Test Group  | Module : L2 |
| Test Case ID  | 003 |
| Priority  | High|

**Pre-Conditions :**
None

**Dependencies :**
None

**User Interaction :**
If user chose to run the test in interactive mode, then the test case has to be selected via console.

#### Test Procedure :

| Variation / Steps | Description | Test Data | Expected Result | Notes |
| ----------------- | ----------- | --------- | --------------- | ------|
| 01 | Get video sink IDs using `getVideoSinkIds`| ids = empty vector | status.isOk() = true | Should be successful |
| 02 | Register event listener using `registerEventListener`| listener = new VideoSinkEventListenerImpl(), registered = false  | regStatus.isOk() = true, registered = true | Should be successful |
| 03 | Open video sink using `open`| controllerListener = new VideoSinkControllerListenerImpl(), controller = nullptr | openStatus.isOk() = true, controller != nullptr | Should be successful |
| 04 | Wait for state transition from CLOSED to OPENING | listener->waitForState(State::CLOSED, State::OPENING) | waitForState() = true | Should be successful |
| 05 | Wait for state transition from OPENING to READY | listener->waitForState(State::OPENING, State::READY)      | waitForState() = true   | Should be successful |
| 06 | Get current state using `getState`      | currentState = State::READY   | getStateStatus.isOk() = true, currentState = State::READY | Should be successful |
| 07 | Start video sink using `start`      | startStatus = controller->start()      | startStatus.isOk() = true   | Should be successful |
| 08 | Wait for state transition from READY to STARTING  | listener->waitForState(State::READY, State::STARTING)     | waitForState() = true   | Should be successful |
| 09 | Wait for state transition from STARTING to STARTED     | listener->waitForState(State::STARTING, State::STARTED)     | waitForState() = true   | Should be successful |
| 10 | Get current state using `getState`      | currentState = State::STARTED   | getStateStatus.isOk() = true, currentState = State::STARTED | Should be successful |
| 11 | Flush video sink using `flush`      | flushStatus = controller->flush(false)     | flushStatus.isOk() = true   | Should be successful |
| 12 | Wait for state transition from STARTED to FLUSHING     | listener->waitForState(State::STARTED, State::FLUSHING)     | waitForState() = true   | Should be successful |
| 13 | Wait for state transition from FLUSHING to STARTED     | listener->waitForState(State::FLUSHING, State::STARTED)     | waitForState() = true   | Should be successful |
| 14 | Get current state using `getState`      | currentState = State::STARTED   | getStateStatus.isOk() = true, currentState = State::STARTED | Should be successful |
| 15 | Stop video sink using `stop`   | startStatus = controller->stop()       | startStatus.isOk() = true   | Should be successful |
| 16 | Wait for state transition from STARTED to STOPPING     | listener->waitForState(State::STARTED, State::STOPPING)     | waitForState() = true   | Should be successful |
| 17 | Wait for state transition from STOPPING to READY  | listener->waitForState(State::STOPPING, State::READY)     | waitForState() = true   | Should be successful |
| 18 | Get current state using `getState`      | currentState = State::READY   | stateStatus.isOk() = true, currentState = State::READY  | Should be successful |
| 19 | Close video sink using `close`      | closed = false        | closeStatus.isOk() = true, closed = true   | Should be successful |
| 20 | Wait for state transition from READY to CLOSING | listener->waitForState(State::READY, State::CLOSING)      | waitForState() = true   | Should be successful |
| 21 | Wait for state transition from CLOSING to CLOSED  | listener->waitForState(State::CLOSING, State::CLOSED)     | waitForState() = true   | Should be successful |
| 22 | Get current state using `getState`      | currentState = State::CLOSED    | getStateStatus.isOk() = true, currentState = State::CLOSED  | Should be successful |
| 23 | Clean up by closing each controller and clearing the environment | None       | None     | Should be successful |

### Test 4

| Title |Details |
| ----- |------- |
| Test  Name | `TestStateTransitions_multipleInstance` |
| Description | Read supported video sinks using IVideoSinkManager class calling getVideoSinkIds. Loop through supported video sinks, register event listeners using registerEventListener. Ensure state transitions (opening-> Ready-> Starting-> started -> Flushing-> stopping -> closing -> Closed). Call open in all IVideoSinkController to transition to the state Ready first. Observe the OnStateChangeEvent in the IVideoSinkEventListener to get OPENING and READY states. Once in READY state query the Sink state using getState and verify. Call start in IVideoSinkController to transition to the state STARTED. Observe the OnStateChangeEvent in the IVideoSinkEventListener to get STARTING and STARTED states. Once in STARTED state query the Sink state using getState and verify. Call flush in IVideoSinkController to go through transitioning state FLUSHING. Observe the OnStateChangeEvent in the IVideoSinkEventListener to get FLUSHING and STARTED states. Once in STARTED state query the Sink state using getState and verify. Call close in IVideoSinkController to transition to the state CLOSED. Observe the OnStateChangeEvent in the IVideoSinkEventListener to get STOPPING, CLOSING and CLOSED states. Once in CLOSED state query the Sink state using getState and verify. |
| Test Group  | Module : L2 |
| Test Case ID  | 004 |
| Priority  | High|

**Pre-Conditions :**
None

**Dependencies :**
None

**User Interaction :**
If user chose to run the test in interactive mode, then the test case has to be selected via console.

#### Test Procedure :

| Variation / Steps | Description | Test Data | Expected Result | Notes |
| ----------------- | ----------- | --------- | --------------- | ------|
| 01 | Get video sink IDs using `getVideoSinkIds`| ids = empty vector | status.isOk() = true | Should be successful |
| 02 | Register event listener using `registerEventListener`| listener = new VideoSinkEventListenerImpl(), registered = false  | regStatus.isOk() = true, registered = true | Should be successful |
| 03 | Open video sink using `open`| controllerListener = new VideoSinkControllerListenerImpl(), controller = nullptr | openStatus.isOk() = true, controller != nullptr | Should be successful |
| 04 | Wait for state transition from CLOSED to OPENING | listener->waitForState(State::CLOSED, State::OPENING) | waitForState() = true | Should be successful |
| 05 | Wait for state transition from OPENING to READY | listener->waitForState(State::OPENING, State::READY)      | waitForState() = true   | Should be successful |
| 06 | Get current state using `getState`      | currentState = State::READY   | getStateStatus.isOk() = true, currentState = State::READY | Should be successful |
| 07 | Start all video sink using `start` by looping though available sinksIds | startStatus = controller->start()      | startStatus.isOk() = true   | Should be successful |
| 08 | Wait for state transition from READY to STARTING  | listener->waitForState(State::READY, State::STARTING)     | waitForState() = true   | Should be successful |
| 09 | Wait for state transition from STARTING to STARTED     | listener->waitForState(State::STARTING, State::STARTED)     | waitForState() = true   | Should be successful |
| 10 | Get current state using `getState`      | currentState = State::STARTED   | getStateStatus.isOk() = true, currentState = State::STARTED | Should be successful |
| 11 | Flush video sink using `flush`      | flushStatus = controller->flush(false)     | flushStatus.isOk() = true   | Should be successful |
| 12 | Wait for state transition from STARTED to FLUSHING     | listener->waitForState(State::STARTED, State::FLUSHING)     | waitForState() = true   | Should be successful |
| 13 | Wait for state transition from FLUSHING to STARTED     | listener->waitForState(State::FLUSHING, State::STARTED)     | waitForState() = true   | Should be successful |
| 14 | Get current state using `getState`      | currentState = State::STARTED   | getStateStatus.isOk() = true, currentState = State::STARTED | Should be successful |
| 15 | Stop video sink using `stop`   | startStatus = controller->stop()       | startStatus.isOk() = true   | Should be successful |
| 16 | Wait for state transition from STARTED to STOPPING     | listener->waitForState(State::STARTED, State::STOPPING)     | waitForState() = true   | Should be successful |
| 17 | Wait for state transition from STOPPING to READY  | listener->waitForState(State::STOPPING, State::READY)     | waitForState() = true   | Should be successful |
| 18 | Get current state using `getState`      | currentState = State::READY   | stateStatus.isOk() = true, currentState = State::READY  | Should be successful |
| 19 | Close video sink using `close`      | closed = false        | closeStatus.isOk() = true, closed = true   | Should be successful |
| 20 | Wait for state transition from READY to CLOSING | listener->waitForState(State::READY, State::CLOSING)      | waitForState() = true   | Should be successful |
| 21 | Wait for state transition from CLOSING to CLOSED  | listener->waitForState(State::CLOSING, State::CLOSED)     | waitForState() = true   | Should be successful |
| 22 | Get current state using `getState`      | currentState = State::CLOSED    | getStateStatus.isOk() = true, currentState = State::CLOSED  | Should be successful |
| 23 | Clean up by closing each controller and clearing the environment | None       | None     | Should be successful |

### Test 5

| Title |Details |
| ----- |------- |
| Test  Name | `TestAttachDetachClock` |
| Description | Read supported video sinks using IVideoSinkManager class calling getVideoSinkIds. Loop through supported video sinks, register event listeners using registerEventListener. Verify attach and detach of clock id works in READY state. Verify attach and detach of clock id works in STARTED state |
| Test Group  | Module : L2 |
| Test Case ID  | 005 |
| Priority  | High|

**Pre-Conditions :**
None

**Dependencies :**
None

**User Interaction :**
If user chose to run the test in interactive mode, then the test case has to be selected via console.

#### Test Procedure :

| Variation / Steps | Description | Test Data | Expected Result | Notes |
| ----------------- | ----------- | --------- | --------------- | ------|
| 01 | Get video sink IDs using `getVideoSinkIds`| ids = empty vector | status.isOk() = true | Should be successful |
| 02 | Register event listener using `registerEventListener`| listener = new VideoSinkEventListenerImpl(), registered = false  | regStatus.isOk() = true, registered = true | Should be successful |
| 03 | Open video sink using `open`| controllerListener = new VideoSinkControllerListenerImpl(), controller = nullptr | openStatus.isOk() = true, controller != nullptr | Should be successful |
| 04 | attach the clock id using `attachClock` and verify using `getClock` | clock id | `getClock` should return attached clock id| Should be successful |
| 05 | Start all video sink using `start` by looping though available sinksIds | startStatus = controller->start()      | startStatus.isOk() = true   | Should be successful |
| 06 | attach the clock id using `attachClock` and verify using `getClock` | clock id | `getClock` should return attached clock id| Should be successful |
| 07 | detach the clock id using `detachClock` and verify using `getClock` | clock id | `getClock` should return UNDEFINED clock id| Should be successful |
| 08 | Stop video sink using `stop`   | startStatus = controller->stop()       | startStatus.isOk() = true   | Should be successful |
| 09 | Close video sink using `close`      | closed = false        | closeStatus.isOk() = true, closed = true   | Should be successful |
