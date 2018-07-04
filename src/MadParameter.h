//
//  MadParameter.h
//  MadMapper_oscQUery
//
//  Created by Jonas Fehr on 06/04/2018.
//

#ifndef MadParameter_h
#define MadParameter_h

//#include "ofMain.h"
#include "ofxMidiDevice.h"
#include "ofxOsc.h"

class MadParameter : public ofParameter<float>{
public:
	MadParameter(ofJson parameterValues){		
		this->setOscAddress(parameterValues["FULL_PATH"].get<std::string>());
		this->setName(parameterValues["DESCRIPTION"]);
        if(!parameterValues["RANGE"].is_null() ){
            range.min = parameterValues["RANGE"].at(0)["MIN"].get<float>();
            range.max = parameterValues["RANGE"].at(0)["MAX"].get<float>();
        }
        float valueNormalized = ofMap(parameterValues["VALUE"].at(0), range.min, range.max, 0, 1);
        this->set(valueNormalized);
        
        bSelectable = false;
        if(parameterValues["DESCRIPTION"] == "Opacity") bSelectable = true;
        bIsGroup = false;
		
		checkIfOpacityParameter(this->getOscAddress());
    };
    
    MadParameter(ofJson parameterValues, string name){
        this->setOscAddress(parameterValues["FULL_PATH"].get<std::string>());
        this->setName(name);
        if(!parameterValues["RANGE"].is_null() ){
            range.min = parameterValues["RANGE"].at(0)["MIN"].get<float>();
            range.max = parameterValues["RANGE"].at(0)["MAX"].get<float>();
        }
        float valueNormalized = ofMap(parameterValues["VALUE"].at(0), range.min, range.max, 0, 1);
        this->set(valueNormalized);
        
        bSelectable = false;
        if(parameterValues["DESCRIPTION"] == "Opacity") bSelectable = true;
        bIsGroup = false;
    };

	MadParameter(){};
    ~MadParameter(){};
	
    float getParameterValue(){
        float value = ofMap(this->get(), 0, 1, range.min, range.max);
        return value;
    }
	
	std::string getParameterName(){
		// returns last part of osc address
		// "/bla/foo/blue" returns "blue"
		std::string oscAddress = this->getOscAddress();
		std::vector<std::string> seglist;
		std::stringstream ss(oscAddress);
		std::string segment;
		while(std::getline(ss, segment, '/')){
			seglist.push_back(segment);
		}
		return seglist.at(seglist.size()-1);
	}
    
    bool bSelectable = false;
    bool isSelectable(){return bSelectable;};
    
     ofEvent<ofxOscMessage> oscSendEvent;

	string oscAddress;
    void setOscAddress(string address){ oscAddress = address;}
    string getOscAddress(){ return oscAddress;}

    struct Range{
        float min;
        float max;
    } range;
    
    bool isGroup(){return bIsGroup;}
    void setIsGroup(bool isGroup){bIsGroup = isGroup;}
    bool bIsGroup;

	// Send OSC when parameter changed
	void onParameterChange(float & p){
		this-set(p);
        ofxOscMessage m;
        m.setAddress(oscAddress);
        m.addFloatArg(getParameterValue());
        ofNotifyEvent(oscSendEvent,m,this);
	}
	
	void checkIfOpacityParameter(std::string oscAddress){
		// checks whether this parameter controls opacity
		oscAddress = oscAddress.substr(1, ofToString(oscAddress).size()); // remove start and end
		std::stringstream ss(oscAddress);
		std::string segment;
		std::vector<std::string> segList;
		while(std::getline(ss, segment, '/')){
			segList.push_back(segment);
		}
		segList.pop_back();

		if((*segList.end()) == "opacity"){
			this->isOpacityParameter = true;
		}
	}
	
	void linkMidiComponent(MidiComponent &midiComponent){
		midiComponent.value = this->get();
		midiComponent.value.addListener(this, &MadParameter::onParameterChange);
	}
	
	void unlinkMidiComponent(MidiComponent &midiComponent){
		midiComponent.value.removeListener(this, &MadParameter::onParameterChange);
	}
	
	bool isOpacityParameter = false;
	
};

#endif /* MadParameter_h */

