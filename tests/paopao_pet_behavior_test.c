#include <assert.h>
#include <stdio.h>

#include "../main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_behavior.h"

static void service_emotion_is_cached_while_speaking(void) {
    paopao_pet_behavior_context_t ctx;
    paopao_pet_behavior_init(&ctx, 1000);

    paopao_pet_behavior_set_voice_state(&ctx, PAOPAO_PET_BEHAVIOR_VOICE_SPEAKING, 1100);
    paopao_pet_behavior_decision_t during_speech =
        paopao_pet_behavior_handle_service_trigger(&ctx, PAOPAO_PET_TRIGGER_SERVICE_HAPPY, 1200);

    assert(!during_speech.has_trigger);
    assert(ctx.pending_service_trigger == PAOPAO_PET_TRIGGER_SERVICE_HAPPY);

    paopao_pet_behavior_set_voice_state(&ctx, PAOPAO_PET_BEHAVIOR_VOICE_IDLE, 3000);
    paopao_pet_behavior_decision_t after_reply =
        paopao_pet_behavior_tick(&ctx, 3000);

    assert(after_reply.has_trigger);
    assert(after_reply.trigger == PAOPAO_PET_TRIGGER_SERVICE_HAPPY);
    assert(ctx.pending_service_trigger == PAOPAO_PET_TRIGGER_NONE);
}

static void latest_service_emotion_wins_during_one_reply(void) {
    paopao_pet_behavior_context_t ctx;
    paopao_pet_behavior_init(&ctx, 1000);

    paopao_pet_behavior_set_voice_state(&ctx, PAOPAO_PET_BEHAVIOR_VOICE_THINKING, 1100);
    assert(!paopao_pet_behavior_handle_service_trigger(&ctx, PAOPAO_PET_TRIGGER_SERVICE_HAPPY, 1200).has_trigger);
    assert(!paopao_pet_behavior_handle_service_trigger(&ctx, PAOPAO_PET_TRIGGER_SERVICE_TIRED, 1300).has_trigger);

    paopao_pet_behavior_set_voice_state(&ctx, PAOPAO_PET_BEHAVIOR_VOICE_IDLE, 3000);
    paopao_pet_behavior_decision_t decision = paopao_pet_behavior_tick(&ctx, 3000);

    assert(decision.has_trigger);
    assert(decision.trigger == PAOPAO_PET_TRIGGER_SERVICE_TIRED);
}

static void neutral_service_emotion_is_ignored(void) {
    paopao_pet_behavior_context_t ctx;
    paopao_pet_behavior_init(&ctx, 1000);

    paopao_pet_behavior_set_voice_state(&ctx, PAOPAO_PET_BEHAVIOR_VOICE_SPEAKING, 1100);
    assert(!paopao_pet_behavior_handle_service_trigger(&ctx, PAOPAO_PET_TRIGGER_SERVICE_NEUTRAL, 1200).has_trigger);
    assert(ctx.pending_service_trigger == PAOPAO_PET_TRIGGER_NONE);
}

static void service_emotion_can_play_immediately_when_idle(void) {
    paopao_pet_behavior_context_t ctx;
    paopao_pet_behavior_init(&ctx, 1000);

    paopao_pet_behavior_decision_t decision =
        paopao_pet_behavior_handle_service_trigger(&ctx, PAOPAO_PET_TRIGGER_SERVICE_HAPPY, 1200);

    assert(decision.has_trigger);
    assert(decision.trigger == PAOPAO_PET_TRIGGER_SERVICE_HAPPY);
    assert(ctx.pending_service_trigger == PAOPAO_PET_TRIGGER_NONE);
}

int main(void) {
    service_emotion_is_cached_while_speaking();
    latest_service_emotion_wins_during_one_reply();
    neutral_service_emotion_is_ignored();
    service_emotion_can_play_immediately_when_idle();
    puts("paopao_pet_behavior pending emotion tests passed");
    return 0;
}
