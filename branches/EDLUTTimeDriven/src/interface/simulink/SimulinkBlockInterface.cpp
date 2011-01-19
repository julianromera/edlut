/***************************************************************************
 *                           SimulinkBlockInterface.cpp                    *
 *                           -------------------                           *
 * copyright            : (C) 2010 by Jesus Garrido                        *
 *                                                                         *
 * email                : jgarrido@atc.ugr.es                              *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "../../../include/simulation/Simulation.h"
#include "../../../include/communication/InputBooleanArrayDriver.h"
#include "../../../include/communication/OutputBooleanArrayDriver.h"
#include "../../../include/communication/FileOutputSpikeDriver.h"

#include <ctime>

// Define input parameters
#define PARAMNET ssGetSFcnParam(S,0) 	// Network description file
#define PARAMWEIGHT ssGetSFcnParam(S,1) 	// Weight description file
#define PARAMLOG ssGetSFcnParam(S,2) 	// Log activity output file
#define PARAMINPUT ssGetSFcnParam(S,3) 	// Input map - Vector mapping each input line with an input cell
#define PARAMOUTPUT ssGetSFcnParam(S,4) 	// Output map - Vector mapping each output line with an output cell

SimulinkBlockInterface::SimulinkBlockInterface(): Simul(0), InputDriver(0), OutputDriver(0){

}

SimulinkBlockInterface::~SimulinkBlockInterface() {
	if (this->Simul!=0){
		delete Simul;
	}

	Simul = 0;
}

void SimulinkBlockInterface::InitializeSimulation(SimStruct *S){
	// Initialize the Simulation Object
	// Get the inputs.
	double SimulationTime = 1e100;

	char NetworkFile[128];
	mxGetString(PARAMNET, NetworkFile, 128);

	char WeightFile[128];
	mxGetString(PARAMWEIGHT, WeightFile, 128);

	char LogFile[128];
	mxGetString(PARAMLOG, LogFile, 128);

	srand (time(NULL));

	try {
		this->Simul = new Simulation(NetworkFile, WeightFile, SimulationTime, 0);

		real_T * InputCells = (real_T *)mxGetData(PARAMINPUT);
		unsigned int NumberOfElements = (unsigned int) mxGetNumberOfElements(PARAMINPUT);
		int * IntInputCells = new int [NumberOfElements];

		for (unsigned int i=0; i<NumberOfElements; ++i){
			IntInputCells[i] = (int)(InputCells[i]);
		}

		// Create a new input object to add input spikes
		this->InputDriver = new InputBooleanArrayDriver(NumberOfElements, IntInputCells);
		this->Simul->AddInputSpikeDriver(this->InputDriver);

		real_T * OutputCells = (real_T *) mxGetData(PARAMOUTPUT);
		unsigned int NumberOfElementsOut = (unsigned int) mxGetNumberOfElements(PARAMOUTPUT);
		int * IntOutputCells = new int [NumberOfElementsOut];

		for (unsigned int i=0; i<NumberOfElementsOut; ++i){
			IntOutputCells[i] = (int) OutputCells[i];
		}

		// Create a new output object to get output spike
		this->OutputDriver = new OutputBooleanArrayDriver(NumberOfElementsOut, IntOutputCells);
		this->Simul->AddOutputSpikeDriver(this->OutputDriver);

		Simul->AddMonitorActivityDriver(new FileOutputSpikeDriver(LogFile,false));

	} catch (EDLUTFileException Exc){
		ssPrintf("Error %li: %s in %s\n",Exc.GetErrorNum(),Exc.GetErrorMsg(),Exc.GetTaskMsg());
		ssPrintf("Try %s\n",Exc.GetRepairMsg());
		ssSetErrorStatus(S, "File error in initializing simulation object");
	} catch (EDLUTException Exc){
		ssPrintf("Error %li: %s in %s\n",Exc.GetErrorNum(),Exc.GetErrorMsg(),Exc.GetTaskMsg());
		ssPrintf("Try %s\n",Exc.GetRepairMsg());
		ssSetErrorStatus(S, "Error in initializing simulation object");
		return;
	}
}

void SimulinkBlockInterface::SimulateStep(SimStruct *S, int_T tid){

	if (ssIsSampleHit(S,0,tid)){
		ssPrintf("Getting number of elements\n");

		unsigned int NumberOfElements = (unsigned int) mxGetNumberOfElements(PARAMINPUT);

		ssPrintf("Found %i input signals\n",NumberOfElements);

		bool * InputSignals = new bool [NumberOfElements];

		InputPtrsType      u     = ssGetInputPortSignalPtrs(S,0);
		InputBooleanPtrsType uPtrs = (InputBooleanPtrsType)u;

		ssPrintf("Input pointer obtained\n");

		for (unsigned int i=0; i<NumberOfElements; ++i){
			boolean_T value = *uPtrs[i];
			InputSignals[i] = value;
			ssPrintf("Input signal %i: Value %i\n",i,value);
		}

		// Get current simulation time
		double StepTime = ssGetSampleTime(S, 0);
		double CurrentTime = (double) ssGetT(S);
		double NextTime = CurrentTime+StepTime;

		ssPrintf("Current time is %f and next time is %f\n",CurrentTime,NextTime);

		this->InputDriver->LoadInputs(Simul->GetQueue(),Simul->GetNetwork(), (bool *) InputSignals, CurrentTime);

		ssPrintf("Input signals obtained\n");

		Simul->RunSimulationSlot(NextTime);

		ssPrintf("Simulation step finished\n");

		delete [] InputSignals;
	}
}

void SimulinkBlockInterface::AssignOutputs(SimStruct *S){
	unsigned int NumberOfElements = (unsigned int) mxGetNumberOfElements(PARAMOUTPUT);

	bool * OutputSignals = new bool [NumberOfElements];
	this->OutputDriver->GetBufferedSpikes(OutputSignals);

	boolean_T * u = (boolean_T *) ssGetOutputPortSignal(S,0);

	for (unsigned int i=0; i<NumberOfElements; ++i){
		u[i] = (boolean_T) OutputSignals[i];
	}
}
