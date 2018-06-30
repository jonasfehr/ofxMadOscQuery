//
//  MadParameter.h
//  MadMapper_oscQUery
//
//  Created by Jonas Fehr on 06/04/2018.
//

#ifndef MadParameter_h
#define MadParameter_h

#include "ofMain.h"
#include "ofxOsc.h"


class MadParameter : public ofParameter<float>{
public:
    MadParameter(){
        this->addListener(this, &MadParameter::onParameterChange);
    }
    
    ~MadParameter(){
        this->removeListener(this, &MadParameter::onParameterChange);
    }
    
    
    string oscAddress;
    void setOscAddress(string address){ oscAddress = address;}
    
    // connect the sender from the parent to facilitate on change sending
    
    bool updated = false;
    
    
    // Send OSC when parameter changed
    void update(ofxOscSender & oscSender){
        if(updated){
            ofxOscMessage m;
            m.setAddress(oscAddress);
            m.addFloatArg(get());
            oscSender.sendMessage(m, false);
            cout << "got here send" << endl;
            
            updated = false;
        }
    }
    
    void onParameterChange(float & p){
        updated = true;
        
        cout << "check" << endl;
        
    }
    
};

#endif /* MadParameter_h */

