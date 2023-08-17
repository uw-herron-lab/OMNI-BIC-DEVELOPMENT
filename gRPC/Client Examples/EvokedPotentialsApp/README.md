# An interface for administering evoked potential experiments
========================================================================================================================================================================================================================
This application has two modes:
1. Scanning: A user loads a sequence of source/destination channels and the app goes through all in an automated process.
2. Single Experiment: A user can upload the source/destination channel in the configuration file and/or select specific source/destination channels manually.

## Functionalities
1. **Upload a configuation file**, scanning example at `test_config_ep_scan.json` and single experiment example at `test_config_ep.json`. 
Parameters:
* scanMode: true if scanning mode, false if single experiment mode.
* monopolar: false for bipolar stimulation, true for monopolar stim.
* stimChannelsQueue: list of source channels to scan in sequence. Defaults to first element of list if scanMode = false.
* returnChannelsQueue: list of destination channels to scan in sequence. Defaults to first element of list if scanMode = false.
* stimPeriod: interval to apply stimulation (uS)
* stimAmplitude, stimDuration, stimThreshold: to define stimulation waveform.
* jitterMax: random jitter from range (0, jitterMax) will be added to pause between pulses.
* baselinePeriod: time (uS) from before stimulation onset to include when graphing average pulse.

2. **Start stimulation** under current configuration. While in Single Experiment mode, start is disabled until source/destination channels have been selected or uploaded from the config. 
3. **Stop stimulation at any time.**
4. **Toggle display channels.**
5. **Visualize data stream** of selected channels.
6. **Real time average pulse** of selected channels.
7. **Console logs state of experiment** (configuration parameters, current source/destination channels, timestamp when stimulation is started/cancelled/completed)

## Next Steps
1. Online pre-processing: Potential to add filtering, artifact removal, normalization and/or baseline correction to `getFilteredChanBuffer` in BICManager. 
========================================================================================================================================================================================================================

