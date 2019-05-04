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
//    MadParameter(string name, float value,  float min, float max){
//        this->setName(name);
//        range.min = min;
//        range.max = max;
//        
//        float valueNormalized = ofMap(value, range.min, range.max, 0, 1);
//        this->set(valueNormalized);
//        
//        bSelectable = false;
//        bIsGroup = false;
//    };
    
	MadParameter(ofJson parameterValues, bool doSendOsc = true){
		this->setOscAddress(parameterValues["FULL_PATH"].get<std::string>());
		this->setName(parameterValues["DESCRIPTION"].get<std::string>());
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
        
        this->doSendOsc = doSendOsc;
    };
    
//    MadParameter(ofJson parameterValues, string name){
//        this->setOscAddress(parameterValues["FULL_PATH"].get<std::string>());
//        this->setName(name);
//        if(!parameterValues["RANGE"].is_null() ){
//            range.min = parameterValues["RANGE"].at(0)["MIN"].get<float>();
//            range.max = parameterValues["RANGE"].at(0)["MAX"].get<float>();
//        }
//        float valueNormalized = ofMap(parameterValues["VALUE"].at(0), range.min, range.max, 0, 1);
//        this->set(valueNormalized);
//        
//        bSelectable = false;
//        if(parameterValues["DESCRIPTION"] == "Opacity") bSelectable = true;
//        bIsGroup = false;
//    };

	MadParameter(){};
    ~MadParameter(){};
	
    float getParameterValue(){
        float value = ofMap(this->get(), 0, 1, range.min, range.max);
        return value;
    }
	
	std::string getParameterName(){
		// returns last part of osc address
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
    
    bool updateFromMidi = false;

//    // Send OSC when parameter changed
    void onParameterChange(float & p){
        updateFromMidi = true;
        this-set(p);
        
        if(doSendOsc){
            ofxOscMessage m;
            
            if(this->isOpacityParameter){
                // TODO: Send visible
                auto newAddress = this->oscAddress;
                auto start_position_to_erase = newAddress.find("opacity");
                newAddress.erase(start_position_to_erase, ofToString("opacity").size());
                newAddress += "visible";
                
                m.clear();
                m.setAddress(newAddress);
                if(p>0) m.addIntArg(1);
                else    m.addIntArg(0);
                ofNotifyEvent(oscSendEvent,m,this);
            }
            m.clear();
            m.setAddress(oscAddress);
            m.addFloatArg(getParameterValue());
            ofNotifyEvent(oscSendEvent,m,this);
        }
        updateFromMidi = false;

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
            
            isMaster = true;
            parentName = segList[segList.size()-1];
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
    bool isMaster = false;
    string connectedMedia;
    void setConnectedMediaName( string name ){ this->connectedMedia = name; };
    string getConnectedMediaName(){ return this->connectedMedia; };
    string parentName;
    
    bool doSendOsc;
    
	
};

#endif /* MadParameter_h */

