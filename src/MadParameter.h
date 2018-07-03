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
	
	void linkMidiComponent(MidiComponent &midiComponent){
		midiComponent.value = this->get();
		midiComponent.value.addListener(this, &MadParameter::onParameterChange);
	}
	
	void unlinkMidiComponent(MidiComponent &midiComponent){
		midiComponent.value.removeListener(this, &MadParameter::onParameterChange);
	}
	
};

#endif /* MadParameter_h */

