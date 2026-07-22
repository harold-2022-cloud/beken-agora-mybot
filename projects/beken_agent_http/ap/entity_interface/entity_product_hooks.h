//entity_product_hooks.h
#pragma once

#include "entity_mqtt_dev_dp.h"

typedef struct
{
    void (*on_dev_status_change)(unsigned char status);
    void (*on_dp_obj_received)(Dp_Obj_Collect_t *dp_obj);
} Entity_Product_Hooks_t;

void Entity_Product_Hooks_Register(const Entity_Product_Hooks_t *hooks);
void Entity_Product_Hooks_On_Dev_Status(unsigned char status);
void Entity_Product_Hooks_On_Dp_Received(Dp_Obj_Collect_t *dp_obj);
