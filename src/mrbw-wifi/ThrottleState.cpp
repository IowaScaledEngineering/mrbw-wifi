#include "ThrottleState.h"

ThrottleState::ThrottleState()
{
  this->locCmdStnRef = NULL;
  this->init();
}

void ThrottleState::init()
{
  this->active = false;
  this->locAddr = 0;
  this->isLongAddr = false;
  this->locSpeed = 0;
  this->locRevDirection = false;
  this->locEStop = false;
  for(uint32_t i=0; i<MAX_FUNCTIONS; i++)
    this->locFunctions[i] = false;

  this->locFunctionsGood = false;
  this->lastUpdate = 0;
  if (NULL != this->locCmdStnRef)
    free(this->locCmdStnRef);

  this->locCmdStnRef = NULL;
}


ThrottleState::~ThrottleState()
{
  if (NULL != this->locCmdStnRef)
    free(this->locCmdStnRef);
}

