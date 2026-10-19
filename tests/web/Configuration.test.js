import { mount } from '@vue/test-utils'
import { describe, expect, it } from 'vitest'

import Advanced from '../../src_assets/common/assets/web/configs/tabs/Advanced.vue'
import General from '../../src_assets/common/assets/web/configs/tabs/General.vue'
import Inputs from '../../src_assets/common/assets/web/configs/tabs/Inputs.vue'
import Network from '../../src_assets/common/assets/web/configs/tabs/Network.vue'

const globalOptions = {
  mocks: {
    $t: key => key,
  },
  stubs: {
    Checkbox: true,
    Info: true,
    PlatformLayout: {
      template: '<div><slot name="windows" /></div>',
    },
    Play: true,
    Plus: true,
    Shield: true,
    Trash2: true,
    TriangleAlert: true,
    Undo: true,
  },
}

describe('configuration accessibility', () => {
  it('gives every global prep command input an accessible label', () => {
    const wrapper = mount(General, {
      props: {
        platform: 'windows',
        config: {
          global_prep_cmd: [{ do: 'start', elevated: false, undo: 'stop' }],
        },
      },
      global: globalOptions,
    })

    for (const input of wrapper.findAll('#global_prep_cmd tbody input')) {
      expect(wrapper.get(`label[for="${input.attributes('id')}"]`).classes()).toContain('visually-hidden')
    }
  })

})

describe('network configuration', () => {
  it('uses the default Moonlight port when the configured port is absent', () => {
    const wrapper = mount(Network, {
      props: {
        platform: 'linux',
        config: { upnp: false },
      },
      global: globalOptions,
    })

    const tableText = wrapper.get('table').text()
    expect(tableText).toContain('47984')
    expect(tableText).toContain('47989')
    expect(tableText).toContain('48010')
    expect(tableText).not.toContain('NaN')
  })
})

describe('FEC configuration', () => {
  it.each([0, 1, 20, 99, 100])('accepts %i percent and updates the configuration', async percentage => {
    const config = { fec_percentage: 20 }
    const wrapper = mount(Advanced, {
      props: { platform: 'macos', config },
      global: globalOptions,
    })
    const input = wrapper.get('#fec_percentage')

    await input.setValue(String(percentage))

    expect(input.element.checkValidity()).toBe(true)
    expect(config.fec_percentage).toBe(percentage)
  })

  it.each([-1, 101, 255, 0.5])('marks %s percent as invalid', async percentage => {
    const wrapper = mount(Advanced, {
      props: { platform: 'macos', config: { fec_percentage: 20 } },
      global: globalOptions,
    })
    const input = wrapper.get('#fec_percentage')

    await input.setValue(String(percentage))

    expect(input.element.checkValidity()).toBe(false)
  })
})

describe('gamepad input configuration', () => {
  function mountInputs(gamepadDriver = 'hidmaestro', gamepad = 'auto', controller = 'enabled') {
    return mount(Inputs, {
      props: {
        platform: 'windows',
        config: {
          controller,
          gamepad,
          gamepad_driver: gamepadDriver,
          keybindings: '[]',
          motion_as_ds4: 'enabled',
          touchpad_as_ds4: 'enabled',
          ds4_back_as_touchpad_click: 'enabled',
          keyboard: 'disabled',
          mouse: 'disabled',
        },
      },
      global: {
        mocks: { $t: key => key },
        stubs: { Checkbox: true, VirtualKeyCodeSelect: true },
      },
    })
  }

  it('offers the HID Maestro profiles that Duo can bind', () => {
    const wrapper = mountInputs()

    expect(wrapper.get('#gamepad').findAll('option').map(option => option.attributes('value'))).toEqual([
      'auto', 'x360', 'xone', 'xseries', 'ds4', 'ds5', 'ds5e',
    ])
    expect(wrapper.find('#motion_as_ds4').exists()).toBe(true)
    expect(wrapper.find('#touchpad_as_ds4').exists()).toBe(true)
  })

  it('offers only HID Maestro and None as backends', async () => {
    const wrapper = mountInputs()

    const backend = wrapper.get('#gamepad_driver')
    expect(backend.findAll('option').map(option => option.attributes('value'))).toEqual(['hidmaestro', 'none'])

    await backend.setValue('none')
    expect(wrapper.find('#gamepad').exists()).toBe(false)
    expect(wrapper.find('#motion_as_ds4').exists()).toBe(false)
    expect(wrapper.find('#back_button_timeout').exists()).toBe(false)
    expect(wrapper.find('#keyboard').exists()).toBe(true)
    expect(wrapper.find('#mouse').exists()).toBe(true)
  })

  it('hides the gamepad controls when gamepad input is disabled', () => {
    const wrapper = mountInputs('hidmaestro', 'auto', 'disabled')

    expect(wrapper.find('#controller').exists()).toBe(true)
    expect(wrapper.find('#gamepad').exists()).toBe(false)
    expect(wrapper.find('#back_button_timeout').exists()).toBe(false)
  })
})
