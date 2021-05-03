/**
 * Class definition for flashscreen
 * 
 * The flashscreen class sends program identification details to the serial 
 * output. Intended to be used at initialisation.
 * 
 * !NOTE! - this class requires serial output to have been started up first.
 * 
 * @author  David Argles, d.argles@gmx.com
 * @version 23May2020 16:26h
 * @param   String  prog  The name of the program
 * @param   String  ver   The version number
 * @param   String  build The date and time of the build
 */

class flashscreen
{
  public:

  void message(String prog, String ver, String build)
  {
    // Identify this program
    Serial.println("\nStarting up...");
    Serial.println("________________________\n");
    Serial.print("Program: ");
    Serial.print(prog);
    Serial.print(" v");
    Serial.println(ver);
    Serial.print("Build:   ");
    Serial.println(build);
    Serial.println("");
  }
};
