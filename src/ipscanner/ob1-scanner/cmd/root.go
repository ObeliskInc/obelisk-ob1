package cmd

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net"
	"os"
	"reflect"
	"runtime"
	"strings"
	"text/tabwriter"
	"time"

	"github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"gitlab.com/NebulousLabs/ob1-scanner/scanner"
	"golang.org/x/crypto/ssh"
)

// exit codes
// inspired by sysexits.h
const (
	exitCodeGeneral = 1  // Not in sysexits.h, but is standard practice.
	exitCodeUsage   = 64 // EX_USAGE in sysexits.h
)

// ScanConfig contains the flags for the scan command
type ScanConfig struct {
	subnet  string
	timeout string
	uiuser  string
	uipass  string
}

type UpgradeConfig struct {
	sshuser            string
	sshpass            string
	siaFirmwarePath    string
	decredFirmwarePath string
	detectPath         string
	host               string
	ipAddr             string
	uiuser             string
	uipass             string
}

type IdentifyConfig struct {
	ipAddr             string
	uiuser             string
	uipass             string
}

var FlagJSON bool

// JSONMachines defines the JSON payload for scan func
type JSONMachines struct {
	Status  bool               `json:"status"`
	Payload []*scanner.Obelisk `json:"payload"`
}

// ScanConf is a config for scan
var ScanConf ScanConfig

// UpgradeConf is a config for upgrade
var UpgradeConf UpgradeConfig

// IdentifyConf is a config for upgrade
var IdentifyConf IdentifyConfig

var rootCmd = &cobra.Command{
	Use:   "ob1-scanner",
	Short: "ob1-scanner is a quick Obelisk scanner tool",
	Run:   startDaemonCmd,
}

var versionCmd = &cobra.Command{
	Use:   "version",
	Short: "Print the version of ob1-scanner",
	Long:  "Prints the current binary version of ob1-scanner you are using.",
	Run: func(cmd *cobra.Command, _ []string) {
		logrus.Infof("ob1-scanner Version 0.1.0 %s / %s\n", runtime.GOOS, runtime.GOARCH)
	},
}

var scanCmd = &cobra.Command{
	Use:   "scan [subnet]",
	Short: "Scans a subnet and returns identified machines.",
	Long:  "Uses a netscan tool to scan a subnet, look for API ports, and try to identify a recognized Obelisk.",
	Run:   wrap(scanHandler),
}

var mdnsCmd = &cobra.Command{
	Use:   "mdns",
	Short: "Starts a mDNS client and watches for Obelisk packets.",
	Long:  "Starts a UDP Multicast Listener, and specifically watches for Obelisk packets to identify the packet source.",
	Run:   wrap(mdnsHandler),
}

var upgradeGen1Cmd = &cobra.Command{
	Use:   "upgrade-gen1",
	Short: "Upgrades a Gen 1 Obelisk with passed in firmware.",
	Long:  "SSHs into the Obelisk, and attempts to upgrade the new firmware.",
	Run:   wrap(upgradeGen1Handler),
}

var upgradeGen2Cmd = &cobra.Command{
	Use:   "upgrade-gen2",
	Short: "Triggers a software update on the specified Gen 2 Obelisk.",
	Long:  "Sends a POST to /api/action/softwareUpdate to trigger the machine to install any available updates.  The Obelisk downloads the update files.",
	Run:   wrap(upgradeGen2Handler),
}

var identifyCmd = &cobra.Command{
	Use:   "identify",
	Short: "Causes the Obelisk to visually identify itself by alternately flashing the red and green LEDs.",
	Long:  "Send a POST to /api/action/identifyWithLEDs to cause the Obelisk to visually identify itself by alternately flashing the red and green LEDs.",
	Run:   wrap(identifyHandler),
}

func init() {
	rootCmd.AddCommand(versionCmd)
	rootCmd.AddCommand(scanCmd)
	rootCmd.AddCommand(mdnsCmd)
	rootCmd.AddCommand(upgradeGen1Cmd)
	rootCmd.AddCommand(upgradeGen2Cmd)
	rootCmd.AddCommand(identifyCmd)

	upgradeGen1Cmd.PersistentFlags().StringVarP(&UpgradeConf.host, "host", "i", "", "set host")
	upgradeGen1Cmd.PersistentFlags().StringVarP(&UpgradeConf.siaFirmwarePath, "siaFirmware", "s", "", "firmware path")
	upgradeGen1Cmd.PersistentFlags().StringVarP(&UpgradeConf.decredFirmwarePath, "decredFirmware", "d", "", "firmware path")
	upgradeGen1Cmd.PersistentFlags().StringVarP(&UpgradeConf.detectPath, "detect", "z", "", "detect path")
	upgradeGen1Cmd.PersistentFlags().StringVarP(&UpgradeConf.sshuser, "user", "u", "root", "ssh user")
	upgradeGen1Cmd.PersistentFlags().StringVarP(&UpgradeConf.sshpass, "password", "p", "obelisk", "ssh password")

	upgradeGen2Cmd.PersistentFlags().StringVarP(&UpgradeConf.uiuser, "user", "u", "admin", "GUI user")
	upgradeGen2Cmd.PersistentFlags().StringVarP(&UpgradeConf.uipass, "password", "p", "admin", "GUI password")
	upgradeGen2Cmd.PersistentFlags().StringVarP(&UpgradeConf.ipAddr, "ip", "i", "", "IP address")

	identifyCmd.PersistentFlags().StringVarP(&IdentifyConf.uiuser, "user", "u", "admin", "GUI user")
	identifyCmd.PersistentFlags().StringVarP(&IdentifyConf.uipass, "password", "p", "admin", "GUI password")
	identifyCmd.PersistentFlags().StringVarP(&IdentifyConf.ipAddr, "ip", "i", "", "IP address")

	// scan conf
	scanCmd.PersistentFlags().StringVarP(&ScanConf.timeout, "timeout", "t", "2s", "timeout for port checks and RPC calls")
	// Figure out subnet
	var ipString string
	ip, err := scanner.SubnetFromInterface()
	if err != nil {
		logrus.Info("Error scanning interface: ", err)
	}
	if ip == nil {
		logrus.Info("Could not auto-configure subnet, setting to 192.168.0.1")
		ipString = "192.168.0.1"
	} else {
		ipString = fmt.Sprintf("%s/%s", ip.String(), "24")
	}

	scanCmd.PersistentFlags().StringVarP(&ScanConf.subnet, "subnet", "i", ipString, "timeout for port checks and RPC calls")
	scanCmd.PersistentFlags().StringVarP(&ScanConf.uiuser, "user", "u", "admin", "GUI user")
	scanCmd.PersistentFlags().StringVarP(&ScanConf.uipass, "password", "p", "admin", "GUI password")

	rootCmd.PersistentFlags().BoolVarP(&FlagJSON, "json", "j", false, "set json output")
}

// Execute is calls the root command from cobra
func Execute() {
	// Runs the root cmd, which is startDaemonCmd in our case. Will exit(64) if
	// flags cannot be parsed.
	if err := rootCmd.Execute(); err != nil {
		os.Exit(exitCodeUsage)
	}

}

func startDaemonCmd(cmd *cobra.Command, _ []string) {
	cmd.UsageFunc()(cmd)
}

func detect() (string, error) {
	port := "22"
	logrus.Infof("Setting up SSH info for Obelisk with IP %s", UpgradeConf.host)
	config := &ssh.ClientConfig{
		User:            UpgradeConf.sshuser,
		HostKeyCallback: ssh.InsecureIgnoreHostKey(),
		Auth: []ssh.AuthMethod{
			ssh.Password(UpgradeConf.sshpass),
		},
	}
	// scp detect
	logrus.Info("SCPing detect to upgrade directory...")
	err := moveFile("detect", UpgradeConf.detectPath, "~/", UpgradeConf.host, port, config)
	if err != nil {
		logrus.Error("Error moving file", err)
		return "", err
	}
	// run detect
	tries := 10
	var exitCode string
	for tries > 0 {
		exitCode = executeCmd("./detect", UpgradeConf.host, port, config)
		if strings.Contains(exitCode, "2") {
			logrus.Infof("%s, trying again...", exitCode)
			tries--
		} else {
			break
		}
	}

	logrus.Info("removing detect binary")
	executeCmd("rm detect", UpgradeConf.host, port, config)

	if strings.Contains(exitCode, "3") {
		logrus.Info("Detected SC1!")
		return "SC1", nil
	} else if strings.Contains(exitCode, "4") {
		logrus.Info("Detected DCR1!")
		return "DCR1", nil
	}
	return "", fmt.Errorf("Invalid model: %s", exitCode)
}

func upgradeGen1Handler() {
	if FlagJSON {
		logrus.SetFormatter(&logrus.JSONFormatter{})
	}
	// cmd := []string{
	// 	"test -f /root/.version && mkdir -p /tmp/upgrade || mkdir -p /root/upgrade",
	// 	`test -f /root/.version && echo "/tmp/upgrade" || echo "/root/upgrade"`,
	// 	"test -f /root/.version && cd /tmp/upgrade && gunzip firmware.tar.gz && tar -xf firmware.tar && rm firmware.tar || cd /root/upgrade && gunzip firmware.tar.gz && tar -xf firmware.tar && rm firmware.tar && ./upgrader.sh",
	// }
	cmd := []string{
		"mkdir -p /root/upgrade",
		`echo "/root/upgrade"`,
		"cd /root/upgrade && gunzip firmware.tar.gz && tar -xf firmware.tar && rm firmware.tar && ./upgrade.sh",
		"rm -rf /root/upgrade && /sbin/reboot -f",
	}

	port := "22"
	logrus.Infof("Setting up SSH info for Obelisk with IP %s", UpgradeConf.host)
	config := &ssh.ClientConfig{
		User:            UpgradeConf.sshuser,
		HostKeyCallback: ssh.InsecureIgnoreHostKey(),
		Auth: []ssh.AuthMethod{
			ssh.Password(UpgradeConf.sshpass),
		},
	}
	// mkdir
	logrus.Info("Creating upgrade directory...")
	executeCmd(cmd[0], UpgradeConf.host, port, config)
	// detect
	model, err := detect()
	if err != nil {
		logrus.Errorf("Error identifying machine... %s.", err)
		return
	}
	var fwPath string
	if model == "SC1" {
		fwPath = UpgradeConf.siaFirmwarePath
	} else if model == "DCR1" {
		fwPath = UpgradeConf.decredFirmwarePath
	}
	// get path
	path := executeCmd(cmd[1], UpgradeConf.host, port, config)
	logrus.Infof("Upgrade Path is %s ", path)
	// scp firmware
	logrus.Infof("SCPing %s firmware to upgrade directory...", model)
	err = moveFile("firmware.tar.gz", fwPath, path, UpgradeConf.host, port, config)
	if err != nil {
		logrus.Error("Error moving firmware: ", err)
		return
	}
	// gunzip and tar
	logrus.Info("Gunzip and untar'ing upgrade files...")
	executeCmd(cmd[2], UpgradeConf.host, port, config)
	logrus.Infof("Firmware transfer complete for Obelisk with IP %s... machine should reboot momentarily.", UpgradeConf.host)
	// finally try reboot
	executeCmd(cmd[3], UpgradeConf.host, port, config)
	return
}

// Trigger an update action on the Obelisk (must login first to get sessionid cookie)
func upgradeGen2Handler() {
	if FlagJSON {
		logrus.SetFormatter(&logrus.JSONFormatter{})
	}
	logrus.Infof("upgradeGen2Handler() called: %s/%s", UpgradeConf.uiuser, UpgradeConf.uipass)

	scanner.TriggerGen2Update(UpgradeConf.ipAddr, UpgradeConf.uiuser, UpgradeConf.uipass)
}

func identifyHandler() {
	logrus.Infof("identifyHandler() called!")
	scanner.IdentifyObelisk(IdentifyConf.ipAddr, IdentifyConf.uiuser, IdentifyConf.uipass)
}

func moveFile(fileName, fromPath, toPath, hostname, port string, config *ssh.ClientConfig) error {
	// client := scp.NewClient(fmt.Sprintf("%s:%s", hostname, port), config)
	// err := client.Connect()
	// if err != nil {
	// 	logrus.Debug("error connecting to scp", err)
	// 	return err
	// }
	// f, err := os.Open(fromPath)
	// if err != nil {
	// 	logrus.Debug("error opening file", err)
	// 	return err
	// }
	// defer client.Close()
	// defer f.Close()
	// err = client.CopyFile(f, toPath, "0655")
	// if err != nil {
	// 	if err.Error() == "Process exited with status 1" {
	// 		return nil
	// 	}
	// 	return err
	// }
	// return nil
	client, err := ssh.Dial("tcp", fmt.Sprintf("%s:%s", hostname, port), config)
	if err != nil {
		return err
	}
	content, err := ioutil.ReadFile(fromPath)
	if err != nil {
		return err
	}
	session, err := client.NewSession()
	if err != nil {
		return err
	}
	defer session.Close()
	defer client.Close()
	go func() {
		w, _ := session.StdinPipe()
		defer w.Close()
		fmt.Fprintln(w, "C0755", len(content), fileName)
		w.Write(content)
		fmt.Fprint(w, "\x00") // transfer end with \x00
	}()
	return session.Run("/usr/bin/scp -t " + toPath)
}

func executeCmd(cmd string, hostname string, port string, config *ssh.ClientConfig) string {
	conn, err := ssh.Dial("tcp", fmt.Sprintf("%s:%s", hostname, port), config)
	defer conn.Close()
	if err != nil {
		logrus.Info("Failed to dial", err)
	}

	var stdoutBuffer bytes.Buffer
	var stderrBuffer bytes.Buffer
	session, err := conn.NewSession()
	session.Stdout = &stdoutBuffer
	session.Stderr = &stderrBuffer
	if err != nil {
		logrus.Error("Failed to create session", err)
	}
	defer session.Close()
	err = session.Run(cmd)
	if err != nil {
		// logrus.Errorf("cmd run error: %s | %s", err, stderrBuffer.String())
		return err.Error()
	}
	response := stdoutBuffer.String()
	return response
}

func scanHandler() {
	if FlagJSON {
		logrus.SetFormatter(&logrus.JSONFormatter{})
	}
	timeout, err := time.ParseDuration(ScanConf.timeout)
	if err != nil {
		logrus.Error(err)
		os.Exit(exitCodeUsage)
	}
	machines, err := scanner.Scan(ScanConf.subnet, timeout, ScanConf.uiuser, ScanConf.uipass)
	if err != nil {
		logrus.Error(err)
		os.Exit(exitCodeUsage)
	}
	if FlagJSON {
		j := &JSONMachines{
			Status:  true,
			Payload: machines,
		}
		b, err := json.Marshal(j)
		if err != nil {
			logrus.Error(err)
			return
		}
		fmt.Print(string(b))
	} else {
		tw := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
		fmt.Fprintf(tw, "\n%s\t%s\t%s\t", "IP", "MAC Address", "Model")
		fmt.Fprintf(tw, "\n%s\t%s\t%s\t\n", "----", "----", "----")

		for _, v := range machines {
			fmt.Fprintf(tw, "%v\t%v\t%v\n", v.IP, v.MAC, v.Model)
		}
		tw.Flush()
	}
}

func mdnsHandler() {
	if FlagJSON {
		logrus.SetFormatter(&logrus.JSONFormatter{})
	}
	addr := &net.UDPAddr{
		IP:   net.ParseIP("224.0.0.251"),
		Port: 5353,
	}
	logrus.Info("Starting UDP Multicast Listener...")
	conn, err := net.ListenMulticastUDP("udp", nil, addr)
	if err != nil {
		logrus.Error(err)
	}
	for {
		b := make([]byte, 100)
		_, remoteAddr, err := conn.ReadFromUDP(b)
		if err != nil {
			logrus.Error(err)
			return
		}

		udpString := string(b)
		contains := strings.Contains(udpString, "Obelisk")
		if contains {
			machine := &scanner.Obelisk{
				IP:    remoteAddr.IP,
				MAC:   "Unknown/mDNS",
				Model: "Unknown",
			}
			logrus.Info("udpString=" + udpString)
			if strings.Contains(udpString, "SC1 Slim") {
				machine.Model = "SC1 Slim"
			} else if strings.Contains(udpString, "DCR1 Slim") {
				machine.Model = "DCR1 Slim"
			} else if strings.Contains(udpString, "SC1") {
				machine.Model = "SC1"
			} else if strings.Contains(udpString, "DCR1") {
				machine.Model = "DCR1"
			}
			if FlagJSON {
				j := &JSONMachines{
					Status:  true,
					Payload: []*scanner.Obelisk{machine},
				}
				b, err := json.Marshal(j)
				if err != nil {
					logrus.Error(err)
					return
				}
				fmt.Print(string(b))
			} else {
				tw := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
				fmt.Fprintf(tw, "\n%s\t%s\t%s\t", "IP", "MAC Address", "Model")
				fmt.Fprintf(tw, "\n%s\t%s\t%s\t\n", "----", "----", "----")

				fmt.Fprintf(tw, "%v\t%v\t%v\n", machine.IP, machine.MAC, machine.Model)
				tw.Flush()
			}
		}
	}
}

// wrap wraps a generic command with a check that the command has been
// passed the correct number of arguments. The command must take only strings
// as arguments.
func wrap(fn interface{}) func(*cobra.Command, []string) {
	fnVal, fnType := reflect.ValueOf(fn), reflect.TypeOf(fn)
	if fnType.Kind() != reflect.Func {
		panic("Wrapped func has wrong signature")
	}
	for i := 0; i < fnType.NumIn(); i++ {
		if fnType.In(i).Kind() != reflect.String {
			panic("Wrapped func has wrong input type signature")
		}
	}

	return func(cmd *cobra.Command, args []string) {
		if len(args) != fnType.NumIn() {
			cmd.UsageFunc()(cmd)
			os.Exit(exitCodeUsage)
		}
		argVals := make([]reflect.Value, fnType.NumIn())
		for i := range args {
			argVals[i] = reflect.ValueOf(args[i])
		}
		fnVal.Call(argVals)
	}
}
