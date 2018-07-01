//
//  MadEvent.hpp
//  MadMapperControl
//
//  Created by Frederik Tollund Juutilainen on 01/07/2018.
//

#ifndef MadEvent_hpp
#define MadEvent_hpp

#include "ofMain.h"

class MadEvent : public ofEventArgs{
public:
	float value;
	std::string oscAddress;
	
	MadEvent(){
		value = 0.0f;
		oscAddress = "";
	}
	
	static ofEvent <MadEvent> events;
};

#endif /* MadEvent_hpp */
