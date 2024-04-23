# Display RAW Stream

Sometimes it is handy to check out the data on the screen with the help of the Arduino Serial Monitor and Serial Plotter.
We read the raw binary data from an URLStream.

As output stream we use a CsvOutput: this class transforms the data into CSV and prints the result to Serial. Finally we can use the Arduino Serial Plotter to view the result as chart:

![serial-plotter](https://pschatzmann.github.io/Resources/img/serial-plotter-01.png)
