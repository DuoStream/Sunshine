<template>
  <Navbar></Navbar>
  <div id="content" class="container">
    <h1 class="my-4">{{ $t('index.welcome') }}</h1>
    <p>{{ $t('index.description') }}</p>

    <!-- Fatal Errors Alert -->
    <div class="alert alert-danger my-4" v-if="fancyLogs.some(x => x.level === 'Fatal')">
      <div>
        <div class="d-flex align-items-center mb-3">
          <alert-circle :size="32" class="icon-lg me-3"></alert-circle>
          <div v-html="$t('index.startup_errors')"></div>
        </div>
        <ul class="mb-3">
          <li v-for="v in fancyLogs.filter(x => x.level === 'Fatal')" :key="`${v.timestamp}-${v.value}`">{{v.value}}</li>
        </ul>
        <RouterLink class="btn btn-danger" to="/troubleshooting#logs">
          <file-text :size="18" class="icon"></file-text>
          View Logs
        </RouterLink>
      </div>
    </div>

    <div class="alert alert-warning my-4" v-if="missingPermissions.length">
      <div class="d-flex align-items-center gap-3 mb-2">
        <alert-triangle :size="24" class="icon"></alert-triangle>
        <strong>{{ $t('index.permissions_missing_title') }}</strong>
      </div>
      <p>{{ $t('index.permissions_missing_desc') }}</p>
      <RouterLink class="btn btn-warning" to="/troubleshooting#permissions">
        {{ $t('index.permissions_review') }}
      </RouterLink>
    </div>

    <!-- Version -->
    <div class="card my-4">
      <div class="card-body" v-if="version">
        <h2>Version {{version.version}}</h2>

        <div v-if="loading" class="my-3">
          {{ $t('index.loading_latest') }}
        </div>

        <div class="alert alert-success my-3" v-if="buildVersionIsDirty">
          <package :size="18" class="icon"></package>
          {{ $t('index.version_dirty') }} 🌇
        </div>

        <div class="alert alert-info my-3" v-if="installedVersionNotStable">
          <info :size="18" class="icon"></info>
          {{ $t('index.installed_version_not_stable') }}
        </div>

        <div v-else-if="(!preReleaseBuildAvailable || !notifyPreReleases) && !stableBuildAvailable && !buildVersionIsDirty">
          <div class="alert alert-success my-3">
            <check-circle :size="18" class="icon"></check-circle>
            {{ $t('index.version_latest') }}
          </div>
        </div>

        <div v-if="notifyPreReleases && preReleaseBuildAvailable">
          <div class="alert alert-warning my-3">
            <!-- header row -->
            <div class="d-flex align-items-center justify-content-between gap-3 flex-wrap mb-3">
              <div class="d-flex align-items-center gap-3 flex-wrap">
                <alert-circle :size="18" class="icon"></alert-circle>
                <span>{{ $t('index.new_pre_release') }}</span>
                <h5 class="mb-0">{{ preReleaseVersion.release.name }}</h5>
              </div>
              <a class="btn btn-success flex-shrink-0" :href="preReleaseVersion.release.html_url" target="_blank">
                <download :size="18" class="icon"></download>
                {{ $t('index.download') }}
              </a>
            </div>

            <!-- body row (full width) -->
            <div class="markdown-body release-notes" v-html="convertMarkdownToHtml(preReleaseVersion.release.body)"></div>
          </div>
        </div>

        <div v-if="stableBuildAvailable">
          <div class="alert alert-warning my-3">
            <!-- header row -->
            <div class="d-flex align-items-center justify-content-between gap-3 flex-wrap mb-3">
              <div class="d-flex align-items-center gap-3 flex-wrap">
                <alert-circle :size="18" class="icon"></alert-circle>
                <span>{{ $t('index.new_stable') }}</span>
                <h5 class="mb-0">{{ githubVersion.release.name }}</h5>
              </div>
              <a class="btn btn-success flex-shrink-0" :href="githubVersion.release.html_url" target="_blank">
                <download :size="18" class="icon"></download>
                {{ $t('index.download') }}
              </a>
            </div>

            <!-- body row (full width) -->
            <div class="markdown-body release-notes" v-html="convertMarkdownToHtml(githubVersion.release.body)"></div>
          </div>
        </div>
      </div>
    </div>

    <!-- Resources -->
    <div class="my-4">
      <Resource-Card :installed-version-not-stable="installedVersionNotStable"></Resource-Card>
    </div>
  </div>
</template>

<script>
  import { marked } from 'marked'
  import Navbar from './Navbar.vue'
  import ResourceCard from './ResourceCard.vue'
  import SunshineVersion from './sunshine_version'
  import {
    AlertCircle,
    AlertTriangle,
    FileText,
    Wrench,
    Package,
    Info,
    CheckCircle,
    Download
  } from '@lucide/vue'

  // Configure marked to allow HTML
  marked.setOptions({
    breaks: true,
    gfm: true,
    headerIds: true,
    mangle: false,
    sanitize: false
  });

  console.log("Hello, Sunshine!")
  export default {
    components: {
      Navbar,
      ResourceCard,
      AlertCircle,
      AlertTriangle,
      FileText,
      Wrench,
      Package,
      Info,
      CheckCircle,
      Download
    },
    data() {
      return {
        version: null,
        githubVersion: null,
        notifyPreReleases: false,
        preReleaseVersion: null,
        loading: true,
        logs: null,
        platform: "",
        controllerEnabled: false,
        permissions: [],
      }
    },
    async created() {
      try {
        let config = await fetch("./api/config").then((r) => r.json());
        this.notifyPreReleases = config.notify_pre_releases;
        this.platform = config.platform;
        this.controllerEnabled = config.controller !== "disabled";
        this.version = new SunshineVersion(null, config.version);
        try {
          const response = await fetch('./api/permissions');
          this.permissions = (await response.json()).permissions || [];
        } catch (e) {
          console.error('Failed to fetch permission status:', e);
        }
        console.log("Version: ", this.version.version)
        this.githubVersion = new SunshineVersion(await fetch("https://api.github.com/repos/LizardByte/Sunshine/releases/latest").then((r) => r.json()), null);
        console.log("GitHub Version: ", this.githubVersion.version)
        this.preReleaseVersion = new SunshineVersion((await fetch("https://api.github.com/repos/LizardByte/Sunshine/releases").then((r) => r.json())).find(release => release.prerelease), null);
        console.log("Pre-Release Version: ", this.preReleaseVersion.version)

      } catch (e) {
        console.error(e);
      }
      try {
        this.logs = (await fetch("./api/logs").then(r => r.text()))
      } catch (e) {
        console.error(e);
      }
      this.loading = false;
    },
    computed: {
      /** Return required permissions that the current platform can check and has not granted. */
      missingPermissions() {
        return this.permissions.filter(permission => permission.required && permission.verifiable && permission.status !== 'granted');
      },
      installedVersionNotStable() {
        if (!this.githubVersion || !this.version) {
          return false;
        }
        return this.version.isGreater(this.githubVersion);
      },
      stableBuildAvailable() {
        if (!this.githubVersion || !this.version) {
          return false;
        }
        return this.githubVersion.isGreater(this.version);
      },
      preReleaseBuildAvailable() {
        if (!this.preReleaseVersion || !this.githubVersion || !this.version) {
          return false;
        }
        return this.preReleaseVersion.isGreater(this.version) && this.preReleaseVersion.isGreater(this.githubVersion);
      },
      buildVersionIsDirty() {
        return this.version.version?.split(".").length === 5 &&
          this.version.version.includes("dirty")
      },
      /** Parse the text errors, calculating the text, the timestamp and the level */
      fancyLogs() {
        if (!this.logs) return [];
        let regex = /(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}]):\s/g;
        let rawLogLines = (this.logs.split(regex)).splice(1);
        let logLines = []
        for (let i = 0; i < rawLogLines.length; i += 2) {
          logLines.push({ timestamp: rawLogLines[i], level: rawLogLines[i + 1].split(":")[0], value: rawLogLines[i + 1] });
        }
        return logLines;
      }
    },
    methods: {
      convertMarkdownToHtml(markdown) {
        if (!markdown) return '';
        return marked.parse(markdown);
      }
    }
  }
</script>
