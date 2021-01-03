#include "app.h"
#include "flash.h"

void APP_Run(void)
{
	FLASH_TryUpdate();
	FLASH_RunApplication();
}
