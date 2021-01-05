#include "flash.h"
#include "main.h"
#include "ff.h"
#include "fatfs.h"

#define FLASH_BUFFER_SIZE ((uint16_t)1 * 8192)
#define FLASH_FILE "update.bin"

typedef void (*pFunction)(void);

pFunction JumpToApplication;
uint32_t  JumpAddress;
uint8_t FlashBuffer[FLASH_BUFFER_SIZE] = { 0x00 };
static uint32_t LastPGAddress = APPLICATION_ADDRESS;
static FIL FlashFile;

void FLASH_If_Init(void)
{
  /* Unlock the Program memory */
  HAL_FLASH_Unlock();

  /* Clear all FLASH flags */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
	
  /* Unlock the Program memory */
  HAL_FLASH_Lock();
}

HAL_StatusTypeDef FLASH_If_Erase(uint32_t start)
{
  uint32_t nbrOfPages = 0;
  uint32_t pageError = 0;
  FLASH_EraseInitTypeDef pEraseInit = {0};
  HAL_StatusTypeDef status = HAL_OK;

  /* Unlock the Flash to enable the flash control register access *************/
  HAL_FLASH_Unlock();

  /* Get the sector where start the user flash area */
  nbrOfPages = (USER_FLASH_END_ADDRESS - start) / FLASH_PAGE_SIZE;

  pEraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
  pEraseInit.PageAddress = start;
  pEraseInit.NbPages = nbrOfPages;
  status = HAL_FLASHEx_Erase(&pEraseInit, &pageError);

  /* Lock the Flash to disable the flash control register access (recommended
     to protect the FLASH memory against possible unwanted operation) *********/
  HAL_FLASH_Lock();

  if (status != HAL_OK)
  {
    /* Error occurred while page erase */
    return HAL_ERROR; // FLASHIF_ERASEKO;
  }

  return HAL_OK;
}

HAL_StatusTypeDef FLASH_If_Write(uint32_t destination, uint32_t *p_source, uint32_t length)
{
  /* Unlock the Flash to enable the flash control register access *************/
  HAL_FLASH_Unlock();

  for (uint32_t i = 0; (i < (length / 4)) && (destination <= (USER_FLASH_END_ADDRESS - 4)); i++)
  {
    /* Device voltage range supposed to be [2.7V to 3.6V], the operation will
       be done by word */
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, destination, *(uint32_t*)(p_source + i)) == HAL_OK)
    {
      /* Check the written value */
      if (*(uint32_t*)destination != *(uint32_t*)(p_source + i))
      {
        /* Flash content doesn't match SRAM content */
        return HAL_ERROR; // (FLASHIF_WRITINGCTRL_ERROR);
      }
      /* Increment FLASH destination address */
      destination += 4;
    }
    else
    {
      /* Error occurred while writing data in Flash memory */
      return HAL_ERROR; //(FLASHIF_WRITING_ERROR);
    }
  }

  /* Lock the Flash to disable the flash control register access (recommended
     to protect the FLASH memory against possible unwanted operation) *********/
  HAL_FLASH_Lock();

  return HAL_OK;
}

HAL_StatusTypeDef FLASH_ProgramFlashMemory(void)
{
	__IO uint32_t read_size = 0x00;
	__IO uint32_t tmp_read_size = 0x00;
	uint32_t read_flag = 1;

	/* Erase address init */
	LastPGAddress = APPLICATION_ADDRESS;

	/* While file still contain data */
	while (read_flag == 1)
	{
		/* Read maximum "BUFFERSIZE" Kbyte from the selected file  */
		if (f_read(&FlashFile, FlashBuffer, FLASH_BUFFER_SIZE, (unsigned int*)&read_size) != FR_OK)
		{
			return HAL_ERROR; // DOWNLOAD_FILE_FAIL;
		}

		/* Temp variable */
		tmp_read_size = read_size;

		/* The read data < "FLASH_BUFFER_SIZE" Kbyte */
		if (tmp_read_size < FLASH_BUFFER_SIZE)
		{
			read_flag = 0;
		}

		/* Program flash memory */
		if (FLASH_If_Write(LastPGAddress, (uint32_t*)FlashBuffer, read_size) != HAL_OK)
		{
			return HAL_ERROR; // DOWNLOAD_WRITE_FAIL;
		}

		/* Update last programmed address value */
		LastPGAddress = LastPGAddress + tmp_read_size;
	}

	return HAL_OK;
}

HAL_StatusTypeDef FLASH_TryUpdate(void)
{
	HAL_StatusTypeDef status = HAL_ERROR;

	FATFS fatFs;
	if (f_mount(&fatFs, "", 1) == FR_OK)
	{
		if (f_open(&FlashFile, FLASH_FILE, FA_READ) == FR_OK)
		{	
			/* Initialize Flash */
			FLASH_If_Init();

			/* Erase necessary page to download image */
			if (FLASH_If_Erase(APPLICATION_ADDRESS) == HAL_OK)
			{
				status = FLASH_ProgramFlashMemory();
			}
			
			f_close(&FlashFile);
			f_unlink(FLASH_FILE);
		}
		
		f_mount(NULL, "", 0);
		//SD_disk_initialize(0);
	}
	
	return status;
}

void FLASH_RunApplication(void)
{
	FATFS_UnLinkDriver(USERPath);
	HAL_SPI_DeInit(&hspi1);
	HAL_RCC_DeInit();
	HAL_DeInit();
	
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;
	
  //__HAL_RCC_SYSCFG_CLK_ENABLE();
  //__HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
	
	/* Jump to user application */
	JumpAddress = *(__IO uint32_t*) (APPLICATION_ADDRESS + 4);
	JumpToApplication = (pFunction) JumpAddress;
	/* Initialize user application's Stack Pointer */
#if   (defined ( __GNUC__ ))
	/* Compensation as the Stack Pointer is placed at the very end of RAM */
	__set_MSP((*(__IO uint32_t*) APPLICATION_ADDRESS) - 64);
#else  /* (defined  (__GNUC__ )) */
	__set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);
#endif /* (defined  (__GNUC__ )) */

	JumpToApplication();
}
