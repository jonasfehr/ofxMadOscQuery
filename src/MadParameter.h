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
	MadParameter(ofJson parameterValues
				 ){		
		// Set values
		this->setOscAddress(parameterValues
				 ["FULL_PATH"].get<std::string>());
		this->setName(parameterValues
				 ["DESCRIPTION"]);
		if(!parameterValues
				 ["RANGE"].is_null() ){
			this->setMin(parameterValues
				 ["RANGE"].at(0)["MIN"]);
			this->setMax(parameterValues
				 ["RANGE"].at(0)["MAX"]);
		}
		this->set(parameterValues
				 ["VALUE"].at(0));
	}
    
    MadParameter(ofJson parameterValues, string name){
        this->setOscAddress(parameterValues
                            ["FULL_PATH"].get<std::string>());
        this->setName(name);
        if(!parameterValues
           ["RANGE"].is_null() ){
            this->setMin(parameterValues
                         ["RANGE"].at(0)["MIN"]);
            this->setMax(parameterValues
                         ["RANGE"].at(0)["MAX"]);
        }
        this->set(parameterValues
                  ["VALUE"].at(0));
                     
    };

	
	MadParameter(){};
	
	
	~MadParameter(){
	}
	
	float getParameterValue(){
		return this->get();
	}
    
     ofEvent<ofxOscMessage> oscSendEvent;

	
	
	string oscAddress;
	void setOscAddress(string address){ oscAddress = address;}
	
	// Send OSC when parameter changed
	void onParameterChange(float & p){
		this-set(p);
		cout << "New param at: " + ofToString(this->get()) << endl;
        
        ofxOscMessage m;
        m.setAddress(oscAddress);
        m.addFloatArg(get());
        
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

