# backlight-ctl

This application is for the smooth adjustment of the brightness of the illumination on laptops, where "raw" or "firmware" devices are available.
What we have:
- runs as a service or daemon;
- has an interface for compatibility with init scripts;
- saves and restores the last level;
- is managed without root privileges;
- setting the number of adjustment levels;
- setting the minimum level;
- setting the duration of the transition;
- forced selection of the device;
- turn on / off the backlight. This is useful, for example, when the projector or TV is connected to a laptop and you just need to turn off the laptop's backlight. In this case, "xset dpms force off" does not do what you want.
- stepless brightness control. When the traditional stepped adjustment, the eyes quickly get tired.
