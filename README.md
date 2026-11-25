# Clock

The clock of Proper Desktop: the time in the cities you follow, alarms,
a stopwatch and timers, each a tab of one window.

<img src="screenshots/clock.png" alt="Clock" width="799">

Alarms and timers ring through a small session service, so they go off
with the window closed and are back where they were after a restart.

## Dependencies

Clock uses properdesktop-toolkit; GoogleTest for the tests.

```sh
sudo apt install cmake libgtest-dev
```

## Build and install

```sh
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install
```

## Tests

```sh
cmake -DBUILD_TESTING=ON ..
make
dbus-run-session -- ctest --output-on-failure
```

## License

BSD-3-Clause; see [LICENSE](LICENSE).
