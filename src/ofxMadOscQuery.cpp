#include "ofxMadOscQuery.h"

ofxMadOscQuery::ofxMadOscQuery(){}
ofxMadOscQuery::~ofxMadOscQuery(){
    for(auto & parameter : parameterMap){
        ofRemoveListener(parameter.second.oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
    }
}

void ofxMadOscQuery::setup(string ip, int sendPort, int receivePort){
    this->ip = ip;
    this->sendPort = sendPort;
    this->receivePort = receivePort;
    this->receiveAddress = "http://"+ip+":"+ofToString(8010);
    oscSender.setup(ip, sendPort);
    oscReceiver.setup(receivePort);
}

//--------------------------------------------------------------
ofJson ofxMadOscQuery::receive(){
    ofHttpResponse resp = ofLoadURL(receiveAddress);
	if(resp.data.size() == 0){
		ofLog(OF_LOG_FATAL_ERROR) << "MadMapper not open!" << endl;
		return nullptr;
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
        page.addParameter(addParameter(element["CONTENTS"][keyword], element["DESCRIPTION"]));
    }
    
    if(!page.isEmpty()){
        pages.push_back(page);
    }
}
//--------------------------------------------------------------
void ofxMadOscQuery::createCustomPage(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, std::string fileName){
	ofJson json = ofLoadJson(fileName);
	for(auto& page : json["pages"]){
		std::string name = page["name"];
		MadParameterPage customPage = MadParameterPage(name, midiDevice);

		// Find matching surfaces
		for(auto& element : page["surfaces"]){
			std::string elementName = ofToString(element).substr(1, ofToString(element).size() - 2);
			std::string surfaceName = "/surfaces/" + elementName;

			for(auto& surfaceParam : parameterMap){
				std::string paramName = ofToString(surfaceParam.first);
				if(elementName == "*"){
					// WILDCARD - take all matching element
					if((surfaceParam.first.rfind("/surfaces/", 0) == 0) && !(surfaceParam.first.rfind("/surfaces/selected", 0) == 0)){
						customPage.addParameter(&surfaceParam.second);
					}
				}else if(elementName == paramName){
					// only take the matching
					customPage.addParameter(&surfaceParam.second);
				}else{
					auto parsedName = elementName.substr(2, ofToString(element).size());
					// split by "/"
					std::vector<std::string> seglist;
					std::stringstream ss(paramName);
					std::string segment;
					while(std::getline(ss, segment, '/')){
						seglist.push_back(segment);
					}
					if(seglist[3] == parsedName && seglist[2] != "selected"){
						customPage.addParameter(&surfaceParam.second);
					}
				}
			}
		}
		pages.push_front(customPage);
	}
}

//--------------------------------------------------------------
void ofxMadOscQuery::createSurfacePages(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json){
    std::string name = "surface";
    int idx = 0;
    for(auto & element : json["CONTENTS"]["surfaces"]["CONTENTS"]){
        if(element["DESCRIPTION"] == "Selected"){
            continue; // skip this one
        }
        auto keyword = element["DESCRIPTION"].get<std::string>();//name + "_" + ofToString(idx);
        MadParameterPage page = MadParameterPage(keyword, midiDevice);
        
        // Add parameters
        for(auto& contents : element["CONTENTS"]){
            if(contents["DESCRIPTION"] == "Opacity"){
                page.addParameter(addParameter(contents));
            }
            if(contents["DESCRIPTION"] == "Color"){
                for(auto& color : contents["CONTENTS"]){
                    // Add rgb
                    if(color["DESCRIPTION"] == "Red" || color["DESCRIPTION"] == "Green" || color["DESCRIPTION"] == "Blue"){
                        page.addParameter(addParameter(color));
                    }
                }
            }
            
            if(contents["DESCRIPTION"] == "fx"){
                for(auto& fx : contents["CONTENTS"]){
                    if( fx["DESCRIPTION"] != "FX Type" && fx["TYPE"]=="f"){
                        page.addParameter(addParameter(fx));
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
void ofxMadOscQuery::createMediaPages(std::map<string, MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json){
    for(auto & element : json["CONTENTS"]["medias"]["CONTENTS"]){
        // exceptions not to add
        if(element["DESCRIPTION"] == "Next" || element["DESCRIPTION"] == "Per Type Selection" || element["DESCRIPTION"] == "Previous" || element["DESCRIPTION"] == "Select" || element["DESCRIPTION"] == "Select By Name" || element["DESCRIPTION"] == "Selected"){
            continue; // skip this one
        }
        
        auto keyword = element["DESCRIPTION"].get<std::string>();
        MadParameterPage page = MadParameterPage(keyword, midiDevice);
        
        // Add parameters
        for(auto& contents : element["CONTENTS"]){
            // exception not to add
            if(contents["DESCRIPTION"] == "Resolution" || contents["DESCRIPTION"] == "Assign To Selected Surfaces" || contents["DESCRIPTION"] == "Assign To All Surfaces" || contents["DESCRIPTION"] == "Restart" || contents["DESCRIPTION"] == "Select" || contents["DESCRIPTION"] == "mappingImage"){
                continue; // skip this one
            }
            
            if(contents["TYPE"] == "f"){
                page.addParameter(addParameter(contents));
            }
            
            
            //            if(contents["DESCRIPTION"] == "Opacity"){
            //                page.addParameter(addParameter(contents));
            //            }
            //            if(contents["DESCRIPTION"] == "Color"){
            //                for(auto& color : contents["CONTENTS"]){
            //                    // Add rgb
            //                    if(color["DESCRIPTION"] == "Red" || color["DESCRIPTION"] == "Green" || color["DESCRIPTION"] == "Blue"){
            //                        page.addParameter(addParameter(color));
            //                    }
            //                }
            //            }
            //
            //            if(contents["DESCRIPTION"] == "fx"){
            //                for(auto& fx : contents["CONTENTS"]){
            //                    // Add rgb
            //                    if( fx["DESCRIPTION"] != "FX Type" && fx["TYPE"]=="f"){
            //                        page.addParameter(addParameter(fx));
            //                    }
            //                }
            //            }
        }
        if(!page.isEmpty()){
            pages.insert(std::make_pair(keyword,page));
        }
    }
}
//--------------------------------------------------------------
void ofxMadOscQuery::oscSendToMadMapper(ofxOscMessage &m){
    oscSender.sendMessage(m, false);
}

void ofxMadOscQuery::oscReceiveMessages(){
    while(oscReceiver.hasWaitingMessages()){
        ofxOscMessage m;
        oscReceiver.getNextMessage(m);
        
        if(m.getAddress() == "/medias/select_by_name"){
            lastSelectedMedia = m.getArgAsString(0);
        }
//        else if(m.getAddress() == "/surfaces/select_by_name"){
//            lastSelectedSurface = m.getArgAsString(0);
//        }
    }
}

//--------------------------------------------------------------
MadParameter* ofxMadOscQuery::addParameter(ofJson parameterValues){
    std::string key = parameterValues["FULL_PATH"];
    parameterMap.insert(std::make_pair(key,MadParameter(parameterValues)));
    auto val = &parameterMap.operator[](key);
    
    ofAddListener(val->oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
    
    return val;
}
//--------------------------------------------------------------
MadParameter* ofxMadOscQuery::addParameter(ofJson parameterValues, std::string name){
    std::string key = parameterValues["FULL_PATH"];
    parameterMap.insert(std::make_pair(key, MadParameter(parameterValues, name)));
    auto val = &parameterMap.operator[](key);
    
    ofAddListener(val->oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
    
    return val;
}

