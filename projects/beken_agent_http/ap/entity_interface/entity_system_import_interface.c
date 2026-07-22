//entity_system_import_interface.c
// Product-side system import: chip common Bsp_* binding is owned by chip_bk7258.
// This layer only binds product policy callbacks.
#include "entity_system_import_interface.h"

#include "entity_iot_func.h"
#include "entity_dev_info.h"
#include "entity_product_hooks.h"

static void Dev_Status_Trampoline(unsigned char status)
{
    Entity_Product_Hooks_On_Dev_Status(status);
}

int Entity_Dev_Status_Callback_Interface_Init(void)
{
    Entity_Dev_Cbs_t cbs =
    {
        .State_Callback = Dev_Status_Trampoline,
    };
    Entity_Dev_Cbs_Init(&cbs);
    return 0;
}

void Entity_System_All_Func_Import_Interface_Init(void)
{
    Entity_System_Chip_Common_Init();
    Entity_Dev_Status_Callback_Interface_Init();
}
