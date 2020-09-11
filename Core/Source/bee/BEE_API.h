
#pragma once

#ifdef  BEE_EXPORT
  #define BEE_API __declspec(dllexport)  
#else
  #define BEE_API __declspec(dllimport)  
#endif
