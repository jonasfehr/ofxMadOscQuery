/*
 A page contains a list of MadParameters
 */
#pragma once

#include "MadParameter.h"
#include "ofxMidiDevice.h"


class MadParameterPage{
public:

    ofParameterGroup linkedParamGroup;

    MadParameterPage(std::string name, ofxMidiDevice* midiDevice, bool isSubpage = false, bool isGroup = false){
		this->name = name;
		this->midiDevice = midiDevice;
		ofLog() << "Constructor for " << this->name << " called!" << endl;
        bSubpage = isSubpage;
        bIsGroup = isGroup;

	};
    
    ~MadParameterPage(){
    };
    
    void setValuesOnDevice(ofAbstractParameter &p){
        auto parameter = parameters.begin();
        for(int i = 1; i < range.first; i++){
            parameter++;
        }

        for(int i = 1; i < 9; i++){
            if(parameter != parameters.end()){
                if((*parameter)->updateFromMidi) return;
                    midiDevice->midiComponents["fader_" + ofToString(i)].value.set((*parameter)->get());

                parameter++;
            }else{
                midiDevice->midiComponents["fader_" + ofToString(i)].value.set(0);
            }
        }
    }
	
	void addParameter(MadParameter* parameter){
		std::string paramName = parameter->getParameterName();
		if(this->name != "opacity"){			
			if(paramName == "opacity" || paramName == "red" || paramName == "green" || paramName == "blue"){
				list<MadParameter*>::iterator it = parameters.begin();
				parameters.push_front(parameter);
			}else{
				parameters.push_back(parameter);
			}
		}
		int upper = parameters.size(); // set range
		if(upper > 8)upper = 8;
		range = std::make_pair(1, upper);
	}
	
	void setLowerBound(int lower){
		int upper = lower + 7;
		if(upper > parameters.size()) {
			upper = parameters.size();
			lower = 1;
		}
		range = std::make_pair(lower, upper);
	}
	
	std::string getLatestParameterName(){
		return "sdfdf";
	}
	
	bool isEmpty(){
		return parameters.size() == 0;
	}
	
	std::string getName(){
		return this->name;
	}
	
	std::list<MadParameter*>* getParameters(){
		return &this->parameters;
	}
	
	void cycleForward(){
		if(range.second < parameters.size()){
			ofLog() << "Cycling forwards - new range: " << range.first << " to " << range.second;
			unlinkDevice();
			range.first++;
			range.second++;
			linkDevice();
		}
	}
	
	void cycleBackward(){
		if(range.first > 1){
			ofLog() << "Cycling backwards - new range: " << range.first << " to " << range.second;
			unlinkDevice();
			range.first--;
			range.second--;
			linkDevice();
		}
	}
	
	void linkDevice(){
		auto parameter = parameters.begin();
		for(int i = 1; i < range.first; i++){
			parameter++;
		}
		
        linkedParamGroup.clear();
		for(int i = 1; i < 9; i++){
            if(parameter != parameters.end()){
                (*parameter)->linkMidiComponent(midiDevice->midiComponents["fader_" + ofToString(i)]);
                linkedParamGroup.add(*(*parameter));
                parameter++;
            }else{
                midiDevice->midiComponents["fader_" + ofToString(i)].value.set(0);
            }

		}
        
        ofAddListener(linkedParamGroup.parameterChangedE(),this,&MadParameterPage::setValuesOnDevice);

	}
	
	void unlinkDevice(){
		auto prevParameter = parameters.begin();
		for(int i = 1; i < range.first; i++){
			prevParameter++;
		}
		
		for(int i = 1; i < 9 && (prevParameter != parameters.end()); i++){
			(*prevParameter)->unlinkMidiComponent(midiDevice->midiComponents["fader_" + ofToString(i)]);
			prevParameter++;
		}
        
ofRemoveListener(linkedParamGroup.parameterChangedE(),this,&MadParameterPage::setValuesOnDevice);

	}
	
	std::pair<int,int> getRange(){
		if(parameters.size()>0 && range.first == 0){
			int lower = 1;
			int upper = parameters.size(); // set range
			if(upper > 8)upper = 8;
			range = std::make_pair(lower, upper);
		}
		return range;
	}
    
    bool isSubpage(){return bSubpage;}
    bool isGroup(){return bIsGroup;}
    bool setIsGroup(bool isGroup){bIsGroup = isGroup;}

    std::list<MadParameterPage>::iterator parentPage;

private:
    bool bSubpage;
    bool bIsGroup;
	std::list<MadParameter*> parameters;
	std::string name = "";
	std::pair<int, int> range;
	ofxMidiDevice* midiDevice;
};
