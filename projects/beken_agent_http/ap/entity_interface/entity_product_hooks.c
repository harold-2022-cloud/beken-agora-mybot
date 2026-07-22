//entity_product_hooks.c
#include "entity_product_hooks.h"

static Entity_Product_Hooks_t s_hooks = {0};

void Entity_Product_Hooks_Register(const Entity_Product_Hooks_t *hooks)
{
    if (hooks == 0)
    {
        return;
    }

    if (hooks->on_dev_status_change != 0)
    {
        s_hooks.on_dev_status_change = hooks->on_dev_status_change;
    }

    if (hooks->on_dp_obj_received != 0)
    {
        s_hooks.on_dp_obj_received = hooks->on_dp_obj_received;
    }
}

void Entity_Product_Hooks_On_Dev_Status(unsigned char status)
{
    if (s_hooks.on_dev_status_change != 0)
    {
        s_hooks.on_dev_status_change(status);
    }
}

void Entity_Product_Hooks_On_Dp_Received(Dp_Obj_Collect_t *dp_obj)
{
    if (s_hooks.on_dp_obj_received != 0)
    {
        s_hooks.on_dp_obj_received(dp_obj);
    }
}
