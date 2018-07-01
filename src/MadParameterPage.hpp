/*
 A page contains a list of MadParameters
 */
#pragma once

#include "MadParameter.h"
#include "ofxMidiDevice.h"

class MadParameterPage{
public:
    MadParameterPage(std::string name, ofxMidiDevice* midiDevice){
        this->name = name;
        this->midiDevice = midiDevice;
    };
    
    void addParameter(MadParameter* parameter){
        parameters.push_front(parameter);
        
        // Set range
        int upper = parameters.size();
        
        if(upper > 8){
            upper = 8;
        }
        
        range = std::make_pair(1, upper);
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
    
    void cycleForward(float & p){
        if(range.second < parameters.size() && p == 1){
            ofLog() << "Cycling forwards - new range: " << range.first << " to " << range.second;
            unlinkDevice();
            range.first++;
            range.second++;
            linkDevice();
        }
    }
    
    void cycleBackward(float & p){
        if(range.first > 1 && p == 1){
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
        return range;
    }
    
    void linkCycleControlComponents(MidiComponent &midiComponentForward,MidiComponent &midiComponentBackward){
        midiComponentForward.value.addListener(this, &MadParameterPage::cycleForward);
        midiComponentBackward.value.addListener(this, &MadParameterPage::cycleBackward);
    }
    
    void unlinkCycleControlComponents(MidiComponent &midiComponentForward,MidiComponent &midiComponentBackward){
        midiComponentForward.value.removeListener(this, &MadParameterPage::cycleForward);
        midiComponentBackward.value.removeListener(this, &MadParameterPage::cycleBackward);
    }
    
private:
    std::list<MadParameter*> parameters;
    std::string name = "";
    std::pair<int, int> range;
    ofxMidiDevice* midiDevice;
};
