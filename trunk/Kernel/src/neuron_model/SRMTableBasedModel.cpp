/***************************************************************************
 *                           SRMTableBasedModel.cpp                        *
 *                           -------------------                           *
 * copyright            : (C) 2010 by Jesus Garrido                        *
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

#include "../../include/neuron_model/SRMTableBasedModel.h"
#include "../../include/neuron_model/NeuronModelTable.h"
#include "../../include/neuron_model/SRMState.h"

#include "../../include/spike/InternalSpike.h"
#include "../../include/spike/Neuron.h"

#include "../../include/simulation/Utils.h"


void SRMTableBasedModel::LoadNeuronModel(string ConfigFile) throw (EDLUTFileException){
	FILE *fh;
	long Currentline = 0L;
	fh=fopen(ConfigFile.c_str(),"rt");
	if(fh){
		Currentline=1L;
		skip_comments(fh,Currentline);
		if(fscanf(fh,"%i",&this->NumStateVar)==1){
			unsigned int nv;

			// Initialize all vectors.
			this->StateVarTable = (NeuronModelTable **) new NeuronModelTable * [this->NumStateVar];
			this->StateVarOrder = (unsigned int *) new unsigned int [this->NumStateVar];

			// Auxiliary table index
			unsigned int * TablesIndex = (unsigned int *) new unsigned int [this->NumStateVar];

			skip_comments(fh,Currentline);
			for(nv=0;nv<this->NumStateVar;nv++){
				if(fscanf(fh,"%i",&TablesIndex[nv])!=1){
					throw EDLUTFileException(13,41,3,1,Currentline);
				}
			}

			skip_comments(fh,Currentline);

			float InitValue;

			// Create a new initial state
			this->InitialState = (SRMState *) new SRMState(this->NumStateVar+3,0,0);
			this->InitialState->SetLastUpdateTime(0);
			this->InitialState->SetNextPredictedSpikeTime(NO_SPIKE_PREDICTED);
			this->InitialState->SetStateVariableAt(0,0);

			for(nv=0;nv<this->NumStateVar;nv++){
				if(fscanf(fh,"%f",&InitValue)!=1){
					throw EDLUTFileException(13,42,3,1,Currentline);
				} else {
					this->InitialState->SetStateVariableAt(nv+1,InitValue);
				}
			}


			skip_comments(fh,Currentline);
			unsigned int FiringIndex;
			if(fscanf(fh,"%i",&FiringIndex)==1){
				skip_comments(fh,Currentline);
				if(fscanf(fh,"%i",&this->NumSynapticVar)==1){
					skip_comments(fh,Currentline);

					this->SynapticVar = (unsigned int *) new unsigned int [this->NumSynapticVar];
					for(nv=0;nv<this->NumSynapticVar;nv++){
						if(fscanf(fh,"%i",&this->SynapticVar[nv])!=1){
							throw EDLUTFileException(13,40,3,1,Currentline);
						}
					}


					skip_comments(fh,Currentline);
					if(fscanf(fh,"%i",&this->LastSpikeVar)!=1){
						throw EDLUTFileException(13,46,3,1,Currentline);
					}

					skip_comments(fh,Currentline);
					if(fscanf(fh,"%i",&this->SeedVar)!=1){
						throw EDLUTFileException(13,47,3,1,Currentline);
					}

					skip_comments(fh,Currentline);
					if(fscanf(fh,"%i",&this->NumTables)==1){
						unsigned int nt;
						int tdeptables[MAXSTATEVARS];
						int tstatevarpos,ntstatevarpos;

						this->Tables = (NeuronModelTable *) new NeuronModelTable [this->NumTables];

						// Update table links
						for(nv=0;nv<this->NumStateVar;nv++){
							this->StateVarTable[nv] = this->Tables+TablesIndex[nv];
						}
						this->FiringTable = this->Tables+FiringIndex;

						for(nt=0;nt<this->NumTables;nt++){
							this->Tables[nt].LoadTableDescription(fh, Currentline);
						}

						this->NumTimeDependentStateVar = 0;
						for(nt=0;nt<this->NumStateVar;nt++){
							for(nv=0;nv<this->StateVarTable[nt]->GetDimensionNumber() && this->StateVarTable[nt]->GetDimensionAt(nv)->statevar != 0;nv++);
							if(nv<this->StateVarTable[nt]->GetDimensionNumber()){
								tdeptables[nt]=1;
								this->NumTimeDependentStateVar++;
							}else{
								tdeptables[nt]=0;
							}
						}

						tstatevarpos=0;
						ntstatevarpos=this->NumTimeDependentStateVar; // we place non-t-depentent variables in the end, so that they are evaluated afterwards
						for(nt=0;nt<this->NumStateVar;nt++){
							this->StateVarOrder[(tdeptables[nt])?tstatevarpos++:ntstatevarpos++]=nt;
						}
					}else{
						throw EDLUTFileException(13,37,3,1,Currentline);
					}
				}else{
					throw EDLUTFileException(13,36,3,1,Currentline);
				}
			}else{
				throw EDLUTFileException(13,35,3,1,Currentline);
			}
		}else{
			throw EDLUTFileException(13,34,3,1,Currentline);
		}
	}else{
		throw EDLUTFileException(13,25,13,0,Currentline);
	}
}

void SRMTableBasedModel::UpdateState(NeuronState * State, double CurrentTime){
	double AuxTime = State->GetLastUpdateTime();

	SRMState * StateAux = (SRMState *) State;

	StateAux->AddElapsedTime(CurrentTime-AuxTime);

	TableBasedModel::UpdateState(State, CurrentTime);
}

void SRMTableBasedModel::SynapsisEffect(NeuronState * State, Interconnection * InputConnection){
	float Value = State->GetStateVariableAt(this->SynapticVar[InputConnection->GetType()]+1);
	State->SetStateVariableAt(this->SynapticVar[InputConnection->GetType()]+1,Value+InputConnection->GetWeight()*exp(1.0));
}

double SRMTableBasedModel::NextFiringPrediction(NeuronState * State){
	State->SetStateVariableAt(this->LastSpikeVar+1,((SRMState *) State)->GetLastSpikeTime());
	State->SetStateVariableAt(this->SeedVar+1,rand()%10);
	return this->FiringTable->TableAccess(State);
}

double SRMTableBasedModel::EndRefractoryPeriod(NeuronState * State){
	return 0.0;
}

SRMTableBasedModel::SRMTableBasedModel(string NeuronModelID): TableBasedModel(NeuronModelID){

}

SRMTableBasedModel::~SRMTableBasedModel(){

}

NeuronState * SRMTableBasedModel::InitializeState(){
	return (SRMState *) new SRMState(*((SRMState *) this->InitialState));
}

InternalSpike * SRMTableBasedModel::GenerateNextSpike(InternalSpike *  OutputSpike){

	Neuron * SourceCell = OutputSpike->GetSource();

	NeuronState * CurrentState = SourceCell->GetNeuronState();

	InternalSpike * NextSpike = 0;

	this->UpdateState((SRMState *)CurrentState,OutputSpike->GetTime());

	double PredictedSpike = this->NextFiringPrediction((SRMState *)CurrentState);

	if (PredictedSpike!=NO_SPIKE_PREDICTED){
		PredictedSpike += CurrentState->GetLastUpdateTime();

		NextSpike = new InternalSpike(PredictedSpike,SourceCell);
	}

	SourceCell->GetNeuronState()->SetNextPredictedSpikeTime(PredictedSpike);

	return NextSpike;
}
