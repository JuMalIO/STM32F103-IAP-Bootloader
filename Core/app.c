#include "app.h"
#include "flash.h"

void APP_Init(void)
{
	FLASH_TryUpdate();
}

void APP_Run(void)
{
	FLASH_RunApplication();
}
