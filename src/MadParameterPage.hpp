/*
 A page contains a list of MadParameters
 */
#pragma once

#include "MadParameter.h"
#include "ofxMidiDevice.h"


class MadParameterPage{
public:
    MadParameterPage(std::string name, ofxMidiDevice* midiDevice, bool isSubpage = false){
		this->name = name;
		this->midiDevice = midiDevice;
		ofLog() << "Constructor for " << this->name << " called!" << endl;
        bSubpage = isSubpage;

	};
	
	void addParameter(MadParameter* parameter){
		if(this->name != "opacity"){
			auto par = parameter->getName();
			if(par == this->name || par == "Red" ||
			   par == "Green" || par == "Blue"){
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
		
		for(int i = 1; i < 9 && (parameter != parameters.end()); i++){
			(*parameter)->linkMidiComponent(midiDevice->midiComponents["fader_" + ofToString(i)]);
			parameter++;
		}
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
	
private:
    bool bSubpage;
	std::list<MadParameter*> parameters;
	std::string name = "";
	std::pair<int, int> range;
	ofxMidiDevice* midiDevice;
};
