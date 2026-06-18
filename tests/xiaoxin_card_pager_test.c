#include <assert.h>
#include <stdio.h>

#include "../main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h"

static void home_down_swipe_enters_notifications(void) {
    xiaoxin_card_pager_t pager;
    xiaoxin_card_pager_init(&pager, 412);

    xiaoxin_card_pager_press(&pager, 206, 60);
    xiaoxin_card_pager_drag(&pager, 206, 154);

    assert(xiaoxin_card_pager_current_page(&pager) == XIAOXIN_CARD_PAGE_HOME);
    assert(xiaoxin_card_pager_target_page(&pager) == XIAOXIN_CARD_PAGE_NOTIFICATIONS);
    assert(xiaoxin_card_pager_offset_y(&pager) == 94);

    xiaoxin_card_pager_release(&pager);

    assert(xiaoxin_card_pager_current_page(&pager) == XIAOXIN_CARD_PAGE_NOTIFICATIONS);
    assert(xiaoxin_card_pager_animation(&pager) == XIAOXIN_CARD_ANIMATION_SNAP);
}

static void home_up_swipe_enters_overview(void) {
    xiaoxin_card_pager_t pager;
    xiaoxin_card_pager_init(&pager, 412);

    xiaoxin_card_pager_press(&pager, 206, 352);
    xiaoxin_card_pager_drag(&pager, 206, 258);
    xiaoxin_card_pager_release(&pager);

    assert(xiaoxin_card_pager_current_page(&pager) == XIAOXIN_CARD_PAGE_OVERVIEW);
    assert(xiaoxin_card_pager_animation(&pager) == XIAOXIN_CARD_ANIMATION_SNAP);
}

static void reverse_swipes_return_home(void) {
    xiaoxin_card_pager_t pager;
    xiaoxin_card_pager_init(&pager, 412);

    xiaoxin_card_pager_press(&pager, 206, 60);
    xiaoxin_card_pager_drag(&pager, 206, 154);
    xiaoxin_card_pager_release(&pager);
    assert(xiaoxin_card_pager_current_page(&pager) == XIAOXIN_CARD_PAGE_NOTIFICATIONS);

    xiaoxin_card_pager_press(&pager, 206, 154);
    xiaoxin_card_pager_drag(&pager, 206, 60);
    xiaoxin_card_pager_release(&pager);
    assert(xiaoxin_card_pager_current_page(&pager) == XIAOXIN_CARD_PAGE_HOME);

    xiaoxin_card_pager_press(&pager, 206, 352);
    xiaoxin_card_pager_drag(&pager, 206, 258);
    xiaoxin_card_pager_release(&pager);
    assert(xiaoxin_card_pager_current_page(&pager) == XIAOXIN_CARD_PAGE_OVERVIEW);

    xiaoxin_card_pager_press(&pager, 206, 258);
    xiaoxin_card_pager_drag(&pager, 206, 352);
    xiaoxin_card_pager_release(&pager);
    assert(xiaoxin_card_pager_current_page(&pager) == XIAOXIN_CARD_PAGE_HOME);
}

static void short_vertical_drag_rebounds_to_current_page(void) {
    xiaoxin_card_pager_t pager;
    xiaoxin_card_pager_init(&pager, 412);

    xiaoxin_card_pager_press(&pager, 206, 100);
    xiaoxin_card_pager_drag(&pager, 206, 142);
    xiaoxin_card_pager_release(&pager);

    assert(xiaoxin_card_pager_current_page(&pager) == XIAOXIN_CARD_PAGE_HOME);
    assert(xiaoxin_card_pager_animation(&pager) == XIAOXIN_CARD_ANIMATION_REBOUND);
    assert(xiaoxin_card_pager_offset_y(&pager) == 0);
}

static void horizontal_drag_is_not_a_card_page_drag(void) {
    xiaoxin_card_pager_t pager;
    xiaoxin_card_pager_init(&pager, 412);

    xiaoxin_card_pager_press(&pager, 120, 206);
    xiaoxin_card_pager_drag(&pager, 190, 218);

    assert(!xiaoxin_card_pager_is_dragging(&pager));
    assert(xiaoxin_card_pager_target_page(&pager) == XIAOXIN_CARD_PAGE_HOME);
}

static void long_drag_can_follow_across_the_screen(void) {
    xiaoxin_card_pager_t pager;
    xiaoxin_card_pager_init(&pager, 412);

    xiaoxin_card_pager_press(&pager, 206, 0);
    xiaoxin_card_pager_drag(&pager, 206, 360);

    assert(xiaoxin_card_pager_is_dragging(&pager));
    assert(xiaoxin_card_pager_offset_y(&pager) == 360);

    xiaoxin_card_pager_drag(&pager, 206, 460);
    assert(xiaoxin_card_pager_offset_y(&pager) == 412);
}

static void visual_page_stays_stable_during_continuous_drag(void) {
    xiaoxin_card_pager_t pager;
    xiaoxin_card_pager_init(&pager, 412);

    xiaoxin_card_pager_press(&pager, 206, 10);
    xiaoxin_card_pager_drag(&pager, 206, 80);

    assert(xiaoxin_card_pager_visual_page(&pager) == XIAOXIN_CARD_PAGE_NOTIFICATIONS);

    xiaoxin_card_pager_drag(&pager, 206, 140);
    assert(xiaoxin_card_pager_visual_page(&pager) == XIAOXIN_CARD_PAGE_NOTIFICATIONS);
}

static void card_items_are_priority_sorted(void) {
    const xiaoxin_card_item_t* items = NULL;
    uint8_t count = 0;

    xiaoxin_card_pager_items(XIAOXIN_CARD_PAGE_NOTIFICATIONS, &items, &count);
    assert(count == 4);
    assert(items[0].priority < items[1].priority);
    assert(items[0].title != NULL);

    xiaoxin_card_pager_items(XIAOXIN_CARD_PAGE_OVERVIEW, &items, &count);
    assert(count == 4);
    assert(items[0].priority < items[1].priority);
    assert(items[0].ttl_ms == 0);
}

static void notification_pagination_tracks_current_item(void) {
    xiaoxin_card_pager_t pager;
    xiaoxin_card_pager_init(&pager, 412);

    assert(xiaoxin_card_pager_notification_index(&pager) == 0);
    assert(xiaoxin_card_pager_notification_count(&pager) == 4);
    const xiaoxin_card_item_t* first = xiaoxin_card_pager_current_notification(&pager);
    assert(first != NULL);
    assert(first->priority == 1);

    assert(!xiaoxin_card_pager_notification_prev(&pager));
    assert(xiaoxin_card_pager_notification_index(&pager) == 0);

    assert(xiaoxin_card_pager_notification_next(&pager));
    assert(xiaoxin_card_pager_notification_index(&pager) == 1);
    const xiaoxin_card_item_t* second = xiaoxin_card_pager_current_notification(&pager);
    assert(second != NULL);
    assert(second->priority == 2);

    assert(xiaoxin_card_pager_notification_next(&pager));
    assert(xiaoxin_card_pager_notification_next(&pager));
    assert(xiaoxin_card_pager_notification_index(&pager) == 3);
    assert(!xiaoxin_card_pager_notification_next(&pager));
    assert(xiaoxin_card_pager_notification_index(&pager) == 3);
}

static void non_home_pages_capture_pet_interaction(void) {
    xiaoxin_card_pager_t pager;
    xiaoxin_card_pager_init(&pager, 412);

    assert(xiaoxin_card_pager_allows_pet_interaction(&pager));

    xiaoxin_card_pager_press(&pager, 206, 60);
    xiaoxin_card_pager_drag(&pager, 206, 154);
    xiaoxin_card_pager_release(&pager);

    assert(!xiaoxin_card_pager_allows_pet_interaction(&pager));
}

int main(void) {
    home_down_swipe_enters_notifications();
    home_up_swipe_enters_overview();
    reverse_swipes_return_home();
    short_vertical_drag_rebounds_to_current_page();
    horizontal_drag_is_not_a_card_page_drag();
    long_drag_can_follow_across_the_screen();
    visual_page_stays_stable_during_continuous_drag();
    card_items_are_priority_sorted();
    notification_pagination_tracks_current_item();
    non_home_pages_capture_pet_interaction();
    puts("xiaoxin_card_pager tests passed");
    return 0;
}
