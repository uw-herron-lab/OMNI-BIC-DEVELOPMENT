#pragma once

#include <cppapi/IImplantFactory.h>
#include <cppapi/IImplantListener.h>
#include <cppapi/IStimulationCommand.h>
#include "gtest/gtest.h"
#include "gmock/gmock.h"

class MockIImplantFactory : public cortec::implantapi::IImplantFactory
{
public:
	virtual ~MockIImplantFactory();
	MOCK_METHOD(std::vector<cortec::implantapi::CExternalUnitInfo*>, getExternalUnitInfos, (), (override));
	MOCK_METHOD(cortec::implantapi::CImplantInfo*, getImplantInfo, (const cortec::implantapi::CExternalUnitInfo& externalUnitInfo), (override));
	MOCK_METHOD(bool, isResponsibleFactory, (const cortec::implantapi::CExternalUnitInfo& externalUnitInfo), (const, override));
	MOCK_METHOD(cortec::implantapi::IImplant*, create, (const cortec::implantapi::CExternalUnitInfo& externalUnitInfo, const cortec::implantapi::CImplantInfo& implantInfo), (override));
	MOCK_METHOD(std::string, getLibraryVersion, (), (const, override));
	  

};

