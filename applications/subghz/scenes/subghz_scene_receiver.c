#include "../subghz_i.h"
#include "../views/receiver.h"

static const NotificationSequence subghs_sequence_rx = {
    &message_green_255,

    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,

    &message_delay_50,
    NULL,
};

static void subghz_scene_receiver_update_statusbar(void* context) {
    SubGhz* subghz = context;
    string_t history_stat_str;
    string_init(history_stat_str);
    if(!subghz_history_get_text_space_left(subghz->txrx->history, history_stat_str)) {
        string_t frequency_str;
        string_t modulation_str;

        string_init(frequency_str);
        string_init(modulation_str);

        subghz_get_frequency_modulation(subghz, frequency_str, modulation_str);

        subghz_view_receiver_add_data_statusbar(
            subghz->subghz_receiver,
            string_get_cstr(frequency_str),
            string_get_cstr(modulation_str),
            string_get_cstr(history_stat_str));

        string_clear(frequency_str);
        string_clear(modulation_str);
    } else {
        subghz_view_receiver_add_data_statusbar(
            subghz->subghz_receiver, string_get_cstr(history_stat_str), "", "");
        subghz->state_notifications = SubGhzNotificationStateIDLE;
    }
    string_clear(history_stat_str);
}

void subghz_scene_receiver_callback(SubGhzCustomEvent event, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, event);
}

static void subghz_scene_add_to_history_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    string_t str_buff;
    string_init(str_buff);

    if(subghz_history_add_to_history(
           subghz->txrx->history, decoder_base, subghz->txrx->frequency, subghz->txrx->preset)) {
        subghz_receiver_reset(receiver);
        string_reset(str_buff);

        subghz->state_notifications = SubGhzNotificationStateRxDone;

        subghz_history_get_text_item_menu(
            subghz->txrx->history, str_buff, subghz_history_get_item(subghz->txrx->history) - 1);
        subghz_view_receiver_add_item_to_menu(
            subghz->subghz_receiver,
            string_get_cstr(str_buff),
            subghz_history_get_type_protocol(
                subghz->txrx->history, subghz_history_get_item(subghz->txrx->history) - 1));

        subghz_scene_receiver_update_statusbar(subghz);
    }
    string_clear(str_buff);
    subghz->txrx->rx_key_state = SubGhzRxKeyStateAddKey;
}

void subghz_scene_receiver_on_enter(void* context) {
    SubGhz* subghz = context;

    string_t str_buff;
    string_init(str_buff);

    if(subghz->txrx->rx_key_state == SubGhzRxKeyStateIDLE) {
        subghz->txrx->frequency = subghz_setting_get_default_frequency(subghz->setting);
        subghz->txrx->preset = FuriHalSubGhzPresetOok650Async;
        subghz_history_reset(subghz->txrx->history);
        subghz->txrx->rx_key_state = SubGhzRxKeyStateStart;
    }

    //Load history to receiver
    subghz_view_receiver_exit(subghz->subghz_receiver);
    for(uint8_t i = 0; i < subghz_history_get_item(subghz->txrx->history); i++) {
        string_reset(str_buff);
        subghz_history_get_text_item_menu(subghz->txrx->history, str_buff, i);
        subghz_view_receiver_add_item_to_menu(
            subghz->subghz_receiver,
            string_get_cstr(str_buff),
            subghz_history_get_type_protocol(subghz->txrx->history, i));
        subghz->txrx->rx_key_state = SubGhzRxKeyStateAddKey;
    }
    string_clear(str_buff);
    subghz_scene_receiver_update_statusbar(subghz);
    subghz_view_receiver_set_callback(
        subghz->subghz_receiver, subghz_scene_receiver_callback, subghz);
    subghz_receiver_set_rx_callback(
        subghz->txrx->receiver, subghz_scene_add_to_history_callback, subghz);

    subghz->state_notifications = SubGhzNotificationStateRx;
    if(subghz->txrx->txrx_state == SubGhzTxRxStateRx) {
        subghz_rx_end(subghz);
    };
    if((subghz->txrx->txrx_state == SubGhzTxRxStateIDLE) ||
       (subghz->txrx->txrx_state == SubGhzTxRxStateSleep)) {
        subghz_begin(subghz, subghz->txrx->preset);
        subghz_rx(subghz, subghz->txrx->frequency);
    }
    subghz_view_receiver_set_idx_menu(subghz->subghz_receiver, subghz->txrx->idx_menu_chosen);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdReceiver);
}

bool subghz_scene_receiver_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubGhzCustomEventViewReceiverBack:

            // Stop CC1101 Rx
            subghz->state_notifications = SubGhzNotificationStateIDLE;
            if(subghz->txrx->txrx_state == SubGhzTxRxStateRx) {
                subghz_rx_end(subghz);
                subghz_sleep(subghz);
            };
            subghz->txrx->hopper_state = SubGhzHopperStateOFF;
            subghz->txrx->idx_menu_chosen = 0;
            subghz_receiver_set_rx_callback(subghz->txrx->receiver, NULL, subghz);

            if(subghz->txrx->rx_key_state == SubGhzRxKeyStateAddKey) {
                subghz->txrx->rx_key_state = SubGhzRxKeyStateExit;
                scene_manager_next_scene(subghz->scene_manager, SubGhzSceneNeedSaving);
            } else {
                subghz->txrx->rx_key_state = SubGhzRxKeyStateIDLE;
                subghz->txrx->frequency = subghz_setting_get_default_frequency(subghz->setting);
                subghz->txrx->preset = FuriHalSubGhzPresetOok650Async;
                scene_manager_search_and_switch_to_previous_scene(
                    subghz->scene_manager, SubGhzSceneStart);
            }
            return true;
            break;
        case SubGhzCustomEventViewReceiverOK:
            subghz->txrx->idx_menu_chosen =
                subghz_view_receiver_get_idx_menu(subghz->subghz_receiver);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiverInfo);
            return true;
            break;
        case SubGhzCustomEventViewReceiverConfig:
            subghz->state_notifications = SubGhzNotificationStateIDLE;
            subghz->txrx->idx_menu_chosen =
                subghz_view_receiver_get_idx_menu(subghz->subghz_receiver);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiverConfig);
            return true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(subghz->txrx->hopper_state != SubGhzHopperStateOFF) {
            subghz_hopper_update(subghz);
            subghz_scene_receiver_update_statusbar(subghz);
        }

        switch(subghz->state_notifications) {
        case SubGhzNotificationStateRx:
            notification_message(subghz->notifications, &sequence_blink_cyan_10);
            break;
        case SubGhzNotificationStateRxDone:
            notification_message(subghz->notifications, &subghs_sequence_rx);
            subghz->state_notifications = SubGhzNotificationStateRx;
            break;
        default:
            break;
        }
    }
    return false;
}

void subghz_scene_receiver_on_exit(void* context) {
    UNUSED(context);
}
