/**
 * Appcelerator Titanium - licensed under the Apache Public License 2
 * see LICENSE in the root folder for details on the license.
 * Copyright (c) 2008 Appcelerator, Inc. All Rights Reserved.
 */

#include "growl_osx.h"
#import "GrowlApplicationBridge.h"
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace kroll;
using namespace ti;

namespace ti {
	GrowlOSX::GrowlOSX(SharedBoundObject global) : GrowlBinding(global) {


	}

	GrowlOSX::~GrowlOSX() {
		// TODO Auto-generated destructor stub
	}

	void GrowlOSX::ShowNotification(std::string& title, std::string& description)
	{
		[GrowlApplicationBridge setGrowlDelegate:@""];
		[GrowlApplicationBridge
			 notifyWithTitle:[NSString stringWithCString:title.c_str()]
			 description:[NSString stringWithCString:description.c_str()]
			 notificationName:@"tiNotification"
			 iconData:nil
			 priority:0
			 isSticky:NO
			 clickContext:nil];
	}

	void GrowlOSX::CopyToApp(kroll::Host *host, kroll::Module *module)
	{
		std::string dir = host->GetApplicationHome() + KR_PATH_SEP + "Contents" +
			KR_PATH_SEP + "Frameworks" + KR_PATH_SEP + "Growl.framework";

		if (!FileUtils::IsDirectory(dir))
		{
			NSFileManager *fm = [NSFileManager defaultManager];
			NSString *src = [NSString stringWithFormat:@"%@/Resources/Growl.framework", GetPath()];
			NSString *dest = [NSString stringWithFormat:@"%@/Contents/Frameworks", host->GetApplicationHome().c_str()];
			[fm copyPath:src toPath:dest handler:nil];

			src = [NSString stringWithFormat:@"%@/Resources/Growl Registration Ticket.growlRegDict", GetPath()];
			dest = [NSString stringWithFormat:@"%@/Contents/Resources", host->GetApplicationHome().c_str()];
			[fm copyPath:src toPath:dest handler:nil];
		}
	}
}
