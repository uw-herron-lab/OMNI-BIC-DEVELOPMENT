#pragma once

#ifdef BICCPPWRAPPER_EXPORTS
#define BICCPPWRAPPER_API __declspec(dllexport)
#else
#define BICCPPWRAPPER_API __declspec(dllimport)
#endif

class BICCPPWRAPPER_API bicCppWrapper
{
public:
	bicCppWrapper(void);
	int count = 0;

private:
	
};