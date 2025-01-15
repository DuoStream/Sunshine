<script setup>
import {ref} from 'vue'
import {$tp} from '../../platform-i18n'
import DisplayModesSettings from "./audiovideo/DisplayModesSettings.vue";
import Checkbox from "../../Checkbox.vue";

const props = defineProps([
  'platform',
  'config',
])

const config = ref(props.config)
</script>

<template>
  <div id="audio-video" class="config-page">
    <!-- Disable Audio -->
    <Checkbox class="mb-3"
              id="stream_audio"
              locale-prefix="config"
              v-model="config.stream_audio"
              default="true"
    ></Checkbox>

    <!-- Display Modes -->
    <DisplayModesSettings
        :platform="platform"
        :config="config"
    />
  </div>
</template>

<style scoped>
</style>
