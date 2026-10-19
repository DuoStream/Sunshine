<template>
  <Navbar></Navbar>
  <div id="content" class="container">
    <h1 class="my-4">{{ $t('troubleshooting.troubleshooting') }}</h1>
    <div class="card my-4" v-if="permissions.length || permissionError">
      <div class="card-body">
        <div class="d-flex align-items-center justify-content-between gap-3 flex-wrap mb-2">
          <h2 id="permissions" class="mb-0">{{ $t('troubleshooting.permissions_title') }}</h2>
          <button class="btn btn-outline-secondary" type="button" @click="refreshPermissions">
            <refresh-cw :size="17" class="icon"></refresh-cw>
            {{ $t('troubleshooting.permissions_refresh') }}
          </button>
        </div>
        <p>{{ $t('troubleshooting.permissions_desc') }}</p>
        <div v-if="permissionError" class="alert alert-danger" role="alert">{{ permissionError }}</div>
        <div v-if="permissions.length" class="table-responsive permission-table-shell">
          <table class="table align-middle mb-0">
            <thead>
              <tr>
                <th scope="col">{{ $t('troubleshooting.permissions_name') }}</th>
                <th scope="col">{{ $t('troubleshooting.permissions_status') }}</th>
                <th scope="col">{{ $t('troubleshooting.permissions_requirement') }}</th>
                <th scope="col" class="permission-action-column">
                  <span class="visually-hidden">{{ $t('troubleshooting.permissions_action') }}</span>
                </th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="permission in orderedPermissions" :key="permission.id">
                <th scope="row">
                  <strong>{{ $t('troubleshooting.permission_' + permission.id +
                    (permission.id === 'input' && (platform === 'linux' || platform === 'freebsd') ? '_unix' : '')) }}</strong>
                  <p class="permission-detail mb-0">{{ $t('troubleshooting.permission_' + permission.id + '_desc' +
                    (permission.id === 'input' && (platform === 'linux' || platform === 'freebsd') ? '_unix' : '')) }}</p>
                  <output v-if="permissionHelp === permission.id" class="permission-help d-block mb-0">
                    {{ $t('troubleshooting.permission_' + permission.id + '_help' +
                      (permission.id === 'input' && platform !== 'macos' ? '_' + platform : '')) }}
                  </output>
                </th>
                <td>
                  <span class="status-icon" :class="permissionStatusClass(permission.status)"
                        :title="$t('troubleshooting.permissions_status_' + permission.status)">
                    <check-circle v-if="permission.status === 'granted'" :size="20" aria-hidden="true"></check-circle>
                    <x-circle v-else-if="permission.status === 'denied'" :size="20" aria-hidden="true"></x-circle>
                    <clock-3 v-else-if="permission.status === 'on_use'" :size="20" aria-hidden="true"></clock-3>
                    <alert-circle v-else-if="permission.status === 'not_determined'" :size="20" aria-hidden="true"></alert-circle>
                    <alert-triangle v-else :size="20" aria-hidden="true"></alert-triangle>
                    <span class="visually-hidden">{{ $t('troubleshooting.permissions_status_' + permission.status) }}</span>
                  </span>
                </td>
                <td>
                  <span class="badge" :class="permission.required ? 'text-bg-primary' : 'text-bg-secondary'">
                    {{ $t(permission.required ? 'troubleshooting.permissions_required' : 'troubleshooting.permissions_optional') }}
                  </span>
                </td>
                <td class="permission-action-column">
                  <button v-if="permission.status !== 'granted' && permission.requestable" class="btn btn-outline-primary"
                          type="button" :disabled="permissionBusy === permission.id"
                          @click="requestPermission(permission.id)">
                    {{ $t(permission.id === 'local_network'
                      ? 'troubleshooting.permissions_privacy_settings' : 'troubleshooting.permissions_request') }}
                  </button>
                  <button v-else-if="permission.status !== 'granted'" class="btn btn-outline-primary"
                          type="button" @click="permissionHelp = permissionHelp === permission.id ? '' : permission.id">
                    {{ $t('troubleshooting.permissions_instructions') }}
                  </button>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>
    <!-- Force Close App -->
    <div class="card my-4">
      <div class="card-body">
        <h2 id="close_apps">{{ $t('troubleshooting.force_close') }}</h2>
        <p>{{ $t('troubleshooting.force_close_desc') }}</p>
        <div class="alert alert-success" v-if="closeAppStatus === true">
          <check-circle :size="18" class="icon"></check-circle>
          {{ $t('troubleshooting.force_close_success') }}
        </div>
        <div class="alert alert-danger" v-if="closeAppStatus === false">
          <alert-circle :size="18" class="icon"></alert-circle>
          {{ $t('troubleshooting.force_close_error') }}
        </div>
        <div>
          <button class="btn btn-warning" :disabled="closeAppPressed" @click="closeApp">
            <x-circle :size="18" class="icon"></x-circle>
            {{ $t('troubleshooting.force_close') }}
          </button>
        </div>
      </div>
    </div>
    <!-- Restart Sunshine -->
    <div class="card my-4">
      <div class="card-body">
        <h2 id="restart">{{ $t('troubleshooting.restart_sunshine') }}</h2>
        <p>{{ $t('troubleshooting.restart_sunshine_desc') }}</p>
        <div class="alert alert-success" v-if="restartPressed === true">
          <check-circle :size="18" class="icon"></check-circle>
          {{ $t('troubleshooting.restart_sunshine_success') }}
        </div>
        <div>
          <button class="btn btn-warning" :disabled="restartPressed" @click="restart">
            <refresh-cw :size="18" class="icon"></refresh-cw>
            {{ $t('troubleshooting.restart_sunshine') }}
          </button>
        </div>
      </div>
    </div>
    <!-- Reset XDG Portal restore token -->
    <div class="card my-4" v-if="platform === 'linux' || platform === 'freebsd'">
      <div class="card-body">
        <h2 id="portal_reset">{{ $t('troubleshooting.portal_reset') }}</h2>
        <p>{{ $t('troubleshooting.portal_reset_desc') }}</p>
        <div class="alert alert-success" v-if="portalResetStatus === true">
          <check-circle :size="18" class="icon"></check-circle>
          {{ $t('troubleshooting.portal_reset_success') }}
        </div>
        <div class="alert alert-danger" v-if="portalResetStatus === false">
          <alert-circle :size="18" class="icon"></alert-circle>
          {{ $t('troubleshooting.portal_reset_error') }}
        </div>
        <div>
          <button class="btn btn-warning" :disabled="portalResetPressed" @click="resetPortalToken">
            <rotate-ccw :size="18" class="icon"></rotate-ccw>
            {{ $t('troubleshooting.portal_reset') }}
          </button>
        </div>
      </div>
    </div>
    <!-- Reset persistent display device settings -->
    <div class="card my-4" v-if="platform === 'windows'">
      <div class="card-body">
        <h2 id="dd_reset">{{ $t('troubleshooting.dd_reset') }}</h2>
        <p style="white-space: pre-line">{{ $t('troubleshooting.dd_reset_desc') }}</p>
        <div class="alert alert-success" v-if="ddResetStatus === true">
          <check-circle :size="18" class="icon"></check-circle>
          {{ $t('troubleshooting.dd_reset_success') }}
        </div>
        <div class="alert alert-danger" v-if="ddResetStatus === false">
          <alert-circle :size="18" class="icon"></alert-circle>
          {{ $t('troubleshooting.dd_reset_error') }}
        </div>
        <div>
          <button class="btn btn-warning" :disabled="ddResetPressed" @click="ddResetPersistence">
            <rotate-ccw :size="18" class="icon"></rotate-ccw>
            {{ $t('troubleshooting.dd_reset') }}
          </button>
        </div>
      </div>
    </div>
    <!-- Unpair Clients -->
    <div class="card my-4">
      <div class="card-body">
        <div class="d-flex justify-content-between align-items-center mb-3">
          <h2 id="unpair" class="mb-0">{{ $t('troubleshooting.unpair_title') }}</h2>
          <button class="btn btn-danger" :disabled="unpairAllPressed" @click="unpairAll">
            <trash-2 :size="18" class="icon"></trash-2>
            {{ $t('troubleshooting.unpair_all') }}
          </button>
        </div>
        <p>{{ $t('troubleshooting.unpair_desc') }}</p>
        <div class="alert alert-success d-flex align-items-center" v-if="showApplyMessage">
          <check-circle :size="18" class="icon"></check-circle>
          <div><b>{{ $t('_common.success') }}</b> {{ $t('troubleshooting.unpair_single_success') }}</div>
          <button class="btn btn-success ms-auto" @click="clickedApplyBanner">{{ $t('_common.dismiss') }}</button>
        </div>
        <div class="alert alert-success" v-if="unpairAllStatus === true">
          <check-circle :size="18" class="icon"></check-circle>
          {{ $t('troubleshooting.unpair_all_success') }}
        </div>
        <div class="alert alert-danger" v-if="unpairAllStatus === false">
          <alert-circle :size="18" class="icon"></alert-circle>
          {{ $t('troubleshooting.unpair_all_error') }}
        </div>
      </div>
      <ul class="list-group list-group-flush" v-if="clients && clients.length > 0">
        <li v-for="client in clients" :key="client.uuid" class="list-group-item d-flex align-items-center">
          <div class="flex-grow-1">
            {{ client.name !== "" ? client.name : $t('troubleshooting.unpair_single_unknown') }}
          </div>
          <div class="form-check form-switch ms-2 mb-0">
            <input class="form-check-input" type="checkbox" role="switch"
                   :id="'toggle-' + client.uuid"
                   :checked="client.enabled"
                   :aria-checked="client.enabled.toString()"
                   @change="toggleClient(client.uuid, !client.enabled)">
          </div>
          <button class="btn btn-danger btn-sm ms-2" @click="unpairSingle(client.uuid)">
            <trash-2 :size="18" class="icon"></trash-2>
          </button>
        </li>
      </ul>
      <ul v-else class="list-group list-group-flush">
        <li class="list-group-item p-3 text-center">
          <em>{{ $t('troubleshooting.unpair_single_no_devices') }}</em>
        </li>
      </ul>
    </div>
    <!-- Logs -->
    <div class="card my-4">
      <div class="card-body">
        <h2 id="logs">{{ $t('troubleshooting.logs') }}</h2>
        <div class="d-flex justify-content-between align-items-baseline py-2">
          <p>{{ $t('troubleshooting.logs_desc') }}</p>
          <div class="input-group" style="max-width: 300px">
            <label for="log-filter" class="visually-hidden">{{ $t('troubleshooting.logs_find') }}</label>
            <span class="input-group-text">
              <search :size="18" class="icon"></search>
            </span>
            <input id="log-filter" type="text" class="form-control" v-model="logFilter" :placeholder="$t('troubleshooting.logs_find')" />
          </div>
        </div>
        <div>
          <div class="troubleshooting-logs" ref="logsContainer">
            <div class="log-nav-overlay">
              <div class="log-nav-controls">
                <button class="log-nav-btn" @click="scrollLogsTo('top')" title="Jump to Top">
                  <chevrons-up :size="18" class="icon"></chevrons-up>
                </button>
                <button class="log-nav-btn" @click="navigateToLog('prev')" :disabled="!hasPrevLog" title="Previous Warning/Error">
                  <chevron-up :size="18" class="icon"></chevron-up>
                </button>
                <button class="log-nav-btn" @click="navigateToLog('next')" :disabled="!hasNextLog" title="Next Warning/Error">
                  <chevron-down :size="18" class="icon"></chevron-down>
                </button>
                <button class="log-nav-btn" @click="scrollLogsTo('bottom')" title="Jump to Bottom">
                  <chevrons-down :size="18" class="icon"></chevrons-down>
                </button>
                <button class="log-nav-btn" @click="copyLogs" title="Copy Logs">
                  <check :size="18" class="icon text-success" v-if="logsCopied"></check>
                  <copy :size="18" class="icon" v-else></copy>
                </button>
              </div>
            </div>
            <pre class="mb-0" v-html="highlightedLogs"></pre>
          </div>
        </div>
      </div>
    </div>
  </div>

</template>

<script>
    import Navbar from './Navbar.vue'
    import { apiFetch } from './fetch_utils'
    import {
      AlertCircle,
      AlertTriangle,
      Check,
      CheckCircle,
      ChevronDown,
      ChevronUp,
      ChevronsDown,
      ChevronsUp,
      Clock3,
      Copy,
      Download,
      ExternalLink,
      Gamepad2,
      KeyRound,
      MousePointer2,
      RefreshCw,
      RotateCcw,
      Search,
      ShieldCheck,
      Sparkles,
      Trash2,
      XCircle,
    } from '@lucide/vue'

    const permissionOrder = ['screen_recording', 'input', 'notifications', 'microphone', 'system_audio', 'local_network'];

    export default {
      components: {
        Navbar,
        AlertCircle,
        AlertTriangle,
        Check,
        CheckCircle,
        ChevronDown,
        ChevronUp,
        ChevronsDown,
        ChevronsUp,
        Clock3,
        Copy,
        Download,
        ExternalLink,
        Gamepad2,
        KeyRound,
        MousePointer2,
        RefreshCw,
        RotateCcw,
        Search,
        ShieldCheck,
        Sparkles,
        Trash2,
        XCircle,
      },
      data() {
        return {
          clients: [],
          closeAppPressed: false,
          closeAppStatus: null,
          ddResetPressed: false,
          ddResetStatus: null,
          logsCopied: false,
          logs: 'Loading...',
          logFilter: null,
          logInterval: null,
          portalResetPressed: false,
          portalResetStatus: null,
          permissions: [],
          permissionBusy: '',
          permissionError: '',
          permissionHelp: '',
          restartPressed: false,
          showApplyMessage: false,
          platform: "",
          unpairAllPressed: false,
          unpairAllStatus: null,
          currentLogIndex: -1,
          logLines: [],
        };
      },
      computed: {
        /** Keep known permissions in troubleshooting order on every platform. */
        orderedPermissions() {
          return this.permissions.slice().sort((left, right) => {
            const leftRank = permissionOrder.indexOf(left.id);
            const rightRank = permissionOrder.indexOf(right.id);
            return (leftRank < 0 ? permissionOrder.length : leftRank) -
              (rightRank < 0 ? permissionOrder.length : rightRank);
          });
        },

        actualLogs() {
          if (!this.logFilter) return this.logs;
          const filterLower = this.logFilter.toLowerCase();
          return this.logs
            .split("\n")
            .filter((x) => x.toLowerCase().includes(filterLower))
            .join("\n");
        },

        /**
         * Parse the (possibly multi-line) log output into timestamp-prefixed entries.
         * Each entry starts with: [YYYY-MM-DD HH:MM:SS.mmm]:
         */
        parsedLogEntries() {
          const text = this.actualLogs || '';

          // Match on timestamp tokens, but keep everything between them as the entry body.
          // Using a global exec loop lets us split without losing delimiters and works
          // even when entries span multiple lines.
          const tsRegex = /\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\]:/g;

          const entries = [];
          const matches = Array.from(text.matchAll(tsRegex));

          // If no timestamps are found, treat everything as a single entry.
          if (matches.length === 0) {
            const raw = text.trimEnd();
            if (!raw) return [];
            return [
              {
                index: 0,
                raw,
                level: 'Info',
                cssClass: 'log-line-info',
              },
            ];
          }

          for (let i = 0; i < matches.length; i++) {
            const start = matches[i].index;
            const end = i + 1 < matches.length ? matches[i + 1].index : text.length;
            const raw = text.slice(start, end).trimEnd();
            if (!raw) continue;

            // Determine level based on the *first* level token in the entry.
            // Sunshine logs are typically: "[ts]: Level: message".
            // Some messages may contain additional embedded timestamps, but we treat
            // those as part of the entry content.
            let level = 'Info';
            let cssClass = 'log-line-info';

            if (/\]:\s*Fatal:/i.test(raw)) {
              level = 'Fatal';
              cssClass = 'log-line-fatal';
            } else if (/\]:\s*(Error|Critical):/i.test(raw)) {
              level = 'Error';
              cssClass = 'log-line-error';
            } else if (/\]:\s*Warning:/i.test(raw)) {
              level = 'Warning';
              cssClass = 'log-line-warning';
            } else if (/\]:\s*Debug:/i.test(raw)) {
              level = 'Debug';
              cssClass = 'log-line-debug';
            }

            entries.push({
              index: entries.length,
              raw,
              level,
              cssClass,
            });
          }

          return entries;
        },

        highlightedLogs() {
          const escapeHtml = (s) =>
            s
              .replaceAll('&', '&amp;')
              .replaceAll('<', '&lt;')
              .replaceAll('>', '&gt;')
              .replaceAll('"', '&quot;')
              .replaceAll("'", '&#39;');

          return this.parsedLogEntries
            .map((entry) => {
              const safe = escapeHtml(entry.raw);
              const isSelected = entry.index === this.currentLogIndex;
              const selectedClass = isSelected ? ' log-entry-selected' : '';
              return `<span data-entry-index="${entry.index}" data-log-level="${entry.cssClass}" class="${entry.cssClass}${selectedClass}">${safe}</span>`;
            })
            // Separate entries visually with a newline.
            .join("\n");
        },

        errorWarningEntries() {
          // Only navigate between warnings/errors/fatal/critical
          return this.parsedLogEntries
            .filter((e) => e.level === 'Warning' || e.level === 'Error' || e.level === 'Fatal')
            .map((e) => e.index);
        },

        hasNextLog() {
          const indices = this.errorWarningEntries;
          if (indices.length === 0) return false;
          if (this.currentLogIndex === -1) return true;
          return indices.some((i) => i > this.currentLogIndex);
        },

        hasPrevLog() {
          const indices = this.errorWarningEntries;
          if (indices.length === 0) return false;
          if (this.currentLogIndex === -1) return false;
          return indices.some((i) => i < this.currentLogIndex);
        },

      },
      created() {
        this._logsCopyTimeout = null;
        fetch("/api/config")
          .then((r) => r.json())
          .then((r) => {
            this.platform = r.platform;
            this.refreshPermissions();
          });

        this.logInterval = setInterval(() => {
          this.refreshLogs();
          if (this.platform) this.refreshPermissions();
        }, 5000);
        this.refreshLogs();
        this.refreshClients();
      },
      beforeUnmount() {
        clearInterval(this.logInterval);
        if (this._logsCopyTimeout) clearTimeout(this._logsCopyTimeout);
      },
      methods: {
        /** Refresh the current platform's permission status. */
        async refreshPermissions() {
          try {
            const response = await fetch('./api/permissions');
            if (!response.ok) throw new Error(this.$t('troubleshooting.permissions_error'));
            this.permissions = (await response.json()).permissions || [];
            this.permissionError = '';
          } catch (error) {
            this.permissionError = error.message;
          }
        },
        /** Initiate the selected native permission action. */
        async requestPermission(id) {
          this.permissionBusy = id;
          try {
            const response = await apiFetch('./api/permissions/request', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ id }),
            });
            if (!response.ok) throw new Error(this.$t('troubleshooting.permissions_error'));
            if (id === 'local_network' || (this.platform === 'macos' && (id === 'screen_recording' || id === 'input'))) {
              this.permissionHelp = id;
            }
            await this.refreshPermissions();
          } catch (error) {
            this.permissionError = error.message;
          } finally {
            this.permissionBusy = '';
          }
        },
        refreshLogs() {
          fetch("./api/logs",)
            .then((r) => r.text())
            .then((r) => {
              this.logs = r;
            });
        },
        closeApp() {
          this.closeAppPressed = true;
          apiFetch("./api/apps/close", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            } })
            .then((r) => r.json())
            .then((r) => {
              this.closeAppPressed = false;
              this.closeAppStatus = r.status;
              setTimeout(() => {
                this.closeAppStatus = null;
              }, 5000);
            });
        },
        unpairAll() {
          this.unpairAllPressed = true;
          apiFetch("./api/clients/unpair-all", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            }
           })
            .then((r) => r.json())
            .then((r) => {
              this.unpairAllPressed = false;
              this.unpairAllStatus = r.status;
              setTimeout(() => {
                this.unpairAllStatus = null;
              }, 5000);
              this.refreshClients();
            });
        },
        unpairSingle(uuid) {
          apiFetch("./api/clients/unpair", {
            method: "POST",
            headers: {
              'Content-Type': 'application/json'
            },
            body: JSON.stringify({ uuid })
          }).then(() => {
            this.showApplyMessage = true;
            this.refreshClients();
          });
        },
        toggleClient(uuid, enabled) {
          fetch("./api/clients/update", {
            method: "POST",
            headers: {
              'Content-Type': 'application/json'
            },
            body: JSON.stringify({ uuid, enabled })
          }).then(() => {
            this.refreshClients();
          });
        },
        refreshClients() {
          fetch("./api/clients/list")
            .then((response) => response.json())
            .then((response) => {
              if (response.status === true && response.named_certs && response.named_certs.length) {
                this.clients = response.named_certs.sort((a, b) => {
                  return (a.name.toLowerCase() > b.name.toLowerCase() || a.name === "" ? 1 : -1)
                });
              } else {
                this.clients = [];
              }
            });
        },
        clickedApplyBanner() {
          this.showApplyMessage = false;
        },
        copyLogs() {
          // Copy the filtered view if a filter is active.
          navigator.clipboard.writeText(this.actualLogs).then(() => {
            // Clear any existing reset timer (handles rapid successive clicks).
            if (this._logsCopyTimeout) clearTimeout(this._logsCopyTimeout);

            // Show checkmark feedback.
            this.logsCopied = true;

            // Revert icon after 2 seconds.
            this._logsCopyTimeout = setTimeout(() => {
              this.logsCopied = false;
            }, 2000);
          }).catch((err) => {
            console.error('Failed to copy logs:', err);
          });
        },
        restart() {
          this.restartPressed = true;
          setTimeout(() => {
            this.restartPressed = false;
          }, 5000);
          apiFetch("./api/restart", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            }
          });
        },
        resetPortalToken() {
          this.portalResetPressed = true;
          apiFetch("./api/reset-portal-token", {
            method: "POST",
            headers: {
              "Content-Type": "application/json"
            }
          })
            .then((r) => r.json())
            .then((r) => {
              this.portalResetStatus = r.status;
              if (r.status) {
                apiFetch("./api/restart", {
                  method: "POST",
                  headers: {
                    "Content-Type": "application/json"
                  }
                });
              }
              setTimeout(() => {
                this.portalResetPressed = false;
                this.portalResetStatus = null;
              }, 5000);
            })
            .catch(() => {
              this.portalResetPressed = false;
              this.portalResetStatus = false;
            });
        },
        ddResetPersistence() {
          this.ddResetPressed = true;
          apiFetch("/api/reset-display-device-persistence", {
            method: "POST",
            headers: {
              "Content-Type": "application/json"
            }
          })
            .then((r) => r.json())
            .then((r) => {
              this.ddResetPressed = false;
              this.ddResetStatus = r.status;
              setTimeout(() => {
                this.ddResetStatus = null;
              }, 5000);
            });
        },
        permissionStatusClass(status) {
          if (status === 'granted') return 'status-icon-success';
          if (status === 'denied') return 'status-icon-danger';
          if (status === 'on_use') return 'status-icon-primary';
          if (status === 'not_determined') return 'status-icon-neutral';
          return 'status-icon-warning';
        },
        navigateToLog(direction) {
          const indices = this.errorWarningEntries;
          if (indices.length === 0) return;

          let targetIndex;

          if (direction === 'next') {
            if (this.currentLogIndex === -1) {
              targetIndex = indices[0];
            } else {
              const nextIndices = indices.filter((i) => i > this.currentLogIndex);
              if (nextIndices.length === 0) return;
              targetIndex = nextIndices[0];
            }
          } else if (direction === 'prev') {
            if (this.currentLogIndex === -1) return;
            const prevIndices = indices.filter((i) => i < this.currentLogIndex);
            if (prevIndices.length === 0) return;
            targetIndex = prevIndices[prevIndices.length - 1];
          } else {
            return;
          }

          this.currentLogIndex = targetIndex;

          this.$nextTick(() => {
            const container = this.$refs.logsContainer;
            if (!container) return;

            const el = container.querySelector(`[data-entry-index="${targetIndex}"]`);
            if (!el) return;

            // Ensure it's visible even for tall multi-line entries.
            const containerRect = container.getBoundingClientRect();
            const elRect = el.getBoundingClientRect();
            const relativeTop = elRect.top - containerRect.top;

            container.scrollTop = container.scrollTop + relativeTop - containerRect.height * 0.15;
          });
        },
        scrollLogsTo(where) {
          const container = this.$refs.logsContainer;
          if (!container) return;

          // Reset the selected error/warning index when jumping to top or bottom
          this.currentLogIndex = -1;

          if (where === 'top') {
            container.scrollTo({ top: 0, behavior: 'smooth' });
            return;
          }

          if (where === 'bottom') {
            container.scrollTo({ top: container.scrollHeight, behavior: 'smooth' });
          }
        },
      },
    }
  </script>
