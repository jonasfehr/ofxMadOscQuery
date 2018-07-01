#include "ofxMadOscQuery.h"

void ofxMadOscQuery::setup(string ip, int sendPort, int receivePort){
    this->ip = ip;
    this->sendPort = sendPort;
    this->receivePort = receivePort;
    this->receiveAddress = "http://"+ip+":"+ofToString(8010);
    oscSender.setup(ip, sendPort);
    
//
//    for(auto & page : pages){
//        for (auto & parameter : *page.getParameters()) {
//            ofAddListener(parameter.oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
//        }
//
//    }
}

//--------------------------------------------------------------
ofJson ofxMadOscQuery::receive(){
    ofHttpResponse resp = ofLoadURL(receiveAddress);
    
    // catch if not connected
    
    if(resp.error == "Couldn't connect to server"){
        return;
    }
    std::stringstream ssJSON;
    ssJSON << resp.data;
    ssJSON >> response;
    
    return response;
}

//--------------------------------------------------------------
void ofxMadOscQuery::setupMadParameterFromJson(MadParameter & newParameter, ofJson jsonParameterValues){
    newParameter.setOscAddress(jsonParameterValues["FULL_PATH"].get<std::string>());
    newParameter.setName(jsonParameterValues["DESCRIPTION"]);
    if(! jsonParameterValues["RANGE"].is_null() ){
        newParameter.setMin(jsonParameterValues["RANGE"].at(0)["MIN"]);
        newParameter.setMax(jsonParameterValues["RANGE"].at(0)["MAX"]);
    }
    
    newParameter.set( jsonParameterValues["VALUE"].at(0));
    //        newParameter.addListener(this, &MadOscQuery::sendOsc);
}

//--------------------------------------------------------------
void ofxMadOscQuery::createOpacityPages(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json){
    // Create pages for opacity value for each surface
    std::string keyword = "opacity";
    MadParameterPage page = MadParameterPage(keyword, midiDevice);
    for(auto & element : json["CONTENTS"]["surfaces"]["CONTENTS"]){
        if(element["DESCRIPTION"] == "selected"){
            // Skip this one
            continue;
        }
        // Add element
        page.addParameter(MadParameter(element["CONTENTS"][keyword],element["DESCRIPTION"]));
    }
        
    if(!page.isEmpty()){
        pages.push_back(page);
    }
}

//--------------------------------------------------------------
void ofxMadOscQuery::createSurfacePages(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json){
    std::string name = "surface";
    int idx = 0;
    for(auto & element : json["CONTENTS"]["surfaces"]["CONTENTS"]){
        if(element["DESCRIPTION"] == "selected"){
            continue; // skip this one
        }
        auto keyword = element["DESCRIPTION"].get<std::string>();//name + "_" + ofToString(idx);
        MadParameterPage page = MadParameterPage(keyword, midiDevice);
        
        // Add parameters
        for(auto& contents : element["CONTENTS"]){
            if(contents["DESCRIPTION"] == "Opacity"){
                page.addParameter(MadParameter(contents));
            }
            if(contents["DESCRIPTION"] == "Color"){
                for(auto& color : contents["CONTENTS"]){
                    // Add rgb
                    if(color["DESCRIPTION"] == "Red" || color["DESCRIPTION"] == "Green" || color["DESCRIPTION"] == "Blue"){
                        page.addParameter(MadParameter(color));
                    }
                }
            }
            
            if(contents["DESCRIPTION"] == "fx"){
                for(auto& fx : contents["CONTENTS"]){
                    // Add rgb
                    if( fx["DESCRIPTION"] != "FX Type" && fx["TYPE"]=="f"){
                        page.addParameter(MadParameter(fx));
                    }
                }
            }
        }
        
        
        if(!page.isEmpty()){
            pages.push_back(page);
            idx++;
        }
    }
}



//--------------------------------------------------------------
void ofxMadOscQuery::oscSendToMadMapper(ofxOscMessage &m){
    oscSender.sendMessage(m, false);
}

//--------------------------------------------------------------
MadParameter* ofxMadOscQuery::addParameter(ofJson parameterValues){
	// Add to map
	
	// return reference
}
