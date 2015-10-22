# MQTT Device
I'm building a few simple devices with the PIC32MX processort and the MPIDE env (Arduino like for the PIC32).
The system is using a Chipkit Uno32 or Fubarino Mini board, a WIZ550io board, a DHT22 (Temperature and Humidity sensor)
and some LEDs (to mimic a traffice light).

At the moment this project is mostly for my experimenting with git, uecide and compiling a working MQTT client for use with the cloud.

## Libraries
I've modified the Ethernet library to get the MAC address. The W550io has it's own MAC address so I don't need to set it but it's useful to have as a unique identifier.

## FIXME
A major problem I need to fix is the fact that Arduino like projects need to hard code things like passwords and user IDs into the code. Obviously that's not a good thing when you keep the project under source control.

Another issue is that I have found I need to remove the IPAddress.* files from the compiler home directories. I need to correct my code, put those files back and hopefull that will be resolved.
