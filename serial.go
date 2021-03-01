package deej

import (
	"bufio"
	"errors"
	"fmt"
	"io"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/jacobsa/go-serial/serial"
	"go.uber.org/zap"

	"github.com/omriharel/deej/util"
)

// SerialIO provides a deej-aware abstraction layer to managing serial I/O
type SerialIO struct {
	comPort  string
	baudRate uint

	deej   *Deej
	logger *zap.SugaredLogger

	stopChannel chan bool
	connected   bool
	connOptions serial.OpenOptions
	conn        io.ReadWriteCloser

	lastKnownNumSliders        int
	currentSliderPercentValues []float32
	currentSliderMuteValues    []bool

	sliderMoveConsumers []chan SliderMoveEvent
}

// SliderMoveEvent represents a single slider move captured by deej
type SliderMoveEvent struct {
	SliderID     int
	PercentValue float32
	isMute       bool
	Mute         bool
}

//var expectedLinePattern = regexp.MustCompile(`^(\xFF)[\x00-\xFF]*\n$`)
var expectedLinePattern = regexp.MustCompile(`^FF[^\s]*0A$`)

// NewSerialIO creates a SerialIO instance that uses the provided deej
// instance's connection info to establish communications with the arduino chip
func NewSerialIO(deej *Deej, logger *zap.SugaredLogger) (*SerialIO, error) {
	logger = logger.Named("serial")

	sio := &SerialIO{
		deej:                deej,
		logger:              logger,
		stopChannel:         make(chan bool),
		connected:           false,
		conn:                nil,
		sliderMoveConsumers: []chan SliderMoveEvent{},
	}

	logger.Debug("Created serial i/o instance")

	// respond to config changes
	sio.setupOnConfigReload()

	return sio, nil
}

// Start attempts to connect to our arduino chip
func (sio *SerialIO) Start() error {

	// don't allow multiple concurrent connections
	if sio.connected {
		sio.logger.Warn("Already connected, can't start another without closing first")
		return errors.New("serial: connection already active")
	}

	// set minimum read size according to platform (0 for windows, 1 for linux)
	// this prevents a rare bug on windows where serial reads get congested,
	// resulting in significant lag
	minimumReadSize := 0
	if util.Linux() {
		minimumReadSize = 1
	}

	sio.connOptions = serial.OpenOptions{
		PortName:        sio.deej.config.ConnectionInfo.COMPort,
		BaudRate:        uint(sio.deej.config.ConnectionInfo.BaudRate),
		DataBits:        8,
		StopBits:        1,
		MinimumReadSize: uint(minimumReadSize),
	}

	sio.logger.Debugw("Attempting serial connection",
		"comPort", sio.connOptions.PortName,
		"baudRate", sio.connOptions.BaudRate,
		"minReadSize", minimumReadSize)

	var err error
	sio.conn, err = serial.Open(sio.connOptions)
	if err != nil {

		// might need a user notification here, TBD
		sio.logger.Warnw("Failed to open serial connection", "error", err)
		return fmt.Errorf("open serial connection: %w", err)
	}

	namedLogger := sio.logger.Named(strings.ToLower(sio.connOptions.PortName))

	namedLogger.Infow("Connected", "conn", sio.conn)
	sio.connected = true

	// read lines or await a stop
	go func() {
		connReader := bufio.NewReader(sio.conn)
		lineChannel := sio.readLine(namedLogger, connReader)
		senderChannel := sio.sender(namedLogger)

		for {
			select {
			case <-sio.stopChannel:
				close(senderChannel)
				sio.close(namedLogger)
			case line := <-lineChannel:
				sio.handleLine(namedLogger, line)
			}

		}
	}()

	return nil
}

// Stop signals us to shut down our serial connection, if one is active
func (sio *SerialIO) Stop() {
	if sio.connected {
		sio.logger.Debug("Shutting down serial connection")
		sio.stopChannel <- true
	} else {
		sio.logger.Debug("Not currently connected, nothing to stop")
	}
}

// SubscribeToSliderMoveEvents returns an unbuffered channel that receives
// a sliderMoveEvent struct every time a slider moves
func (sio *SerialIO) SubscribeToSliderMoveEvents() chan SliderMoveEvent {
	ch := make(chan SliderMoveEvent)
	sio.sliderMoveConsumers = append(sio.sliderMoveConsumers, ch)

	return ch
}

func (sio *SerialIO) setupOnConfigReload() {
	configReloadedChannel := sio.deej.config.SubscribeToChanges()

	const stopDelay = 50 * time.Millisecond

	go func() {
		for {
			select {
			case <-configReloadedChannel:

				// make any config reload unset our slider number to ensure process volumes are being re-set
				// (the next read line will emit SliderMoveEvent instances for all sliders)\
				// this needs to happen after a small delay, because the session map will also re-acquire sessions
				// whenever the config file is reloaded, and we don't want it to receive these move events while the map
				// is still cleared. this is kind of ugly, but shouldn't cause any issues
				go func() {
					<-time.After(stopDelay)
					sio.lastKnownNumSliders = 0
				}()

				// if connection params have changed, attempt to stop and start the connection
				if sio.deej.config.ConnectionInfo.COMPort != sio.connOptions.PortName ||
					uint(sio.deej.config.ConnectionInfo.BaudRate) != sio.connOptions.BaudRate {

					sio.logger.Info("Detected change in connection parameters, attempting to renew connection")
					sio.Stop()

					// let the connection close
					<-time.After(stopDelay)

					if err := sio.Start(); err != nil {
						sio.logger.Warnw("Failed to renew connection after parameter change", "error", err)
					} else {
						sio.logger.Debug("Renewed connection successfully")
					}
				}
			}
		}
	}()
}

func (sio *SerialIO) close(logger *zap.SugaredLogger) {
	if err := sio.conn.Close(); err != nil {
		logger.Warnw("Failed to close serial connection", "error", err)
	} else {
		logger.Debug("Serial connection closed")
	}

	sio.conn = nil
	sio.connected = false
}

func (sio *SerialIO) readLine(logger *zap.SugaredLogger, reader *bufio.Reader) chan []byte {
	ch := make(chan []byte)

	go func() {
		for {
			line, err := reader.ReadBytes('\n')
			//.ReadString('\n')
			if err != nil {

				if sio.deej.Verbose() {
					logger.Warnw("Failed to read line from serial", "error", err, "line", line)
				}

				// just ignore the line, the read loop will stop after this
				return
			}

			if sio.deej.Verbose() {
				//logger.Debugw("Read new line", "line", util.BytesToString(line), "ASCII", string(line))
			}

			// deliver the line to the channel
			ch <- line
		}
	}()

	return ch
}

func (sio *SerialIO) handleLine(logger *zap.SugaredLogger, line []byte) {

	// this function receives an unsanitized line which is guaranteed to end with LF,
	// but most lines will end with CRLF. it may also have garbage instead of
	// deej-formatted values, so we must check for that! just ignore bad ones
	var sb strings.Builder
	lineHex := util.BytesToString(line)
	//if !expectedLinePattern.Match(line) {

	if !expectedLinePattern.MatchString(lineHex) {
		if sio.deej.Verbose() {
			logger.Debugw("Discarding line", "line", lineHex, "ASCII", string(line))
		}
		return
	}

	// trim prefix and suffix
	line = line[1 : len(line)-1]

	if len(line) == 0 {
		if sio.deej.Verbose() {
			logger.Debugw("empty command", "line", lineHex)
		}
		return
	}

	//Parse Header

	h := line[0]
	//cmd := (h >> 7) == 1
	numSliders := int(h & 0b00111111)
	if sio.deej.Verbose() {
		//logger.Debugw("command", "cmd", cmd, "nsliders", numSliders)
	}

	// update our slider count, if needed - this will send slider move events for all
	if numSliders != sio.lastKnownNumSliders {
		logger.Infow("Detected sliders", "amount", numSliders)
		sio.lastKnownNumSliders = numSliders
		sio.currentSliderPercentValues = make([]float32, numSliders)
		sio.currentSliderMuteValues = make([]bool, numSliders)

		// reset everything to be an impossible value to force the slider move event later
		for idx := range sio.currentSliderPercentValues {
			sio.currentSliderPercentValues[idx] = -1.0
		}
	}

	line = line[1:]

	if len(line) != numSliders {
		if sio.deej.Verbose() {
			logger.Debugw("Malformed command", "line", lineHex, "nsl", numSliders)
		}
		return
	}

	moveEvents := []SliderMoveEvent{}
	for i, b := range line {
		//Parse volume cmd
		m := (b >> 7) == 1
		v := int(b & 0b01111111)

		if v > 100 {
			sio.logger.Debugw("Volume cannot be greater than zero", "line", lineHex, "volume", v)
			return
		}

		sb.WriteString(strconv.Itoa(v))
		sb.WriteByte(':')
		mch := byte('U')
		if m {
			mch = 'M'
		}
		sb.WriteByte(mch)
		if i < (numSliders - 1) {
			sb.WriteByte('|')
		}

		dirtyFloat := float32(v) / 100

		// normalize it to an actual volume scalar between 0.0 and 1.0 with 2 points of precision
		normalizedScalar := util.NormalizeScalar(dirtyFloat)

		// if sliders are inverted, take the complement of 1.0
		if sio.deej.config.InvertSliders {
			normalizedScalar = 1 - normalizedScalar
		}

		// check if it changes the desired state (could just be a jumpy raw slider value)
		if util.SignificantlyDifferent(sio.currentSliderPercentValues[i], normalizedScalar, sio.deej.config.NoiseReductionLevel) {

			// if it does, update the saved value and create a move event
			sio.currentSliderPercentValues[i] = normalizedScalar

			moveEvents = append(moveEvents, SliderMoveEvent{
				SliderID:     i,
				PercentValue: normalizedScalar,
			})

			if sio.deej.Verbose() {
				logger.Debugw("Slider moved", "event", moveEvents[len(moveEvents)-1])
			}
		}

		sio.currentSliderMuteValues[i] = m

		moveEvents = append(moveEvents, SliderMoveEvent{
			SliderID:     i,
			PercentValue: 0,
			isMute:       true,
			Mute:         m,
		})

	}

	if sio.deej.Verbose() {
		logger.Debugw("Received command", "line", lineHex, "cmd", sb.String())
	}

	// deliver move events if there are any, towards all potential consumers
	if len(moveEvents) > 0 {
		for _, consumer := range sio.sliderMoveConsumers {
			for _, moveEvent := range moveEvents {
				consumer <- moveEvent
			}
		}
	}
}
func (sio *SerialIO) sender(logger *zap.SugaredLogger) chan struct{} {
	ticker := time.NewTicker(200 * time.Millisecond)
	quit := make(chan struct{})
	go func() {
		for {
			select {
			case <-ticker.C:
				sio.sendValues(logger)
			case <-quit:
				ticker.Stop()
				return
			}
		}
	}()

	return quit
}

func (sio *SerialIO) sendValues(logger *zap.SugaredLogger) {
	var sb strings.Builder
	msg := make([]byte, 0, sio.lastKnownNumSliders+3)
	var b byte
	var m byte

	msg = append(msg, 0xFF)

	//Make header
	b = byte(sio.lastKnownNumSliders)
	b |= (1 << 7)
	msg = append(msg, b)

	for i, v := range sio.currentSliderPercentValues {

		b = byte(v * 100)

		mch := byte('U')
		if sio.currentSliderMuteValues[i] {
			m = 1
			mch = 'M'
		} else {
			m = 0
		}

		b |= (m << 7)

		sb.WriteString(strconv.Itoa(int(b)))
		sb.WriteByte(':')
		sb.WriteByte(mch)
		if i < (len(sio.currentSliderPercentValues) - 1) {
			sb.WriteByte('|')
		}

		msg = append(msg, b)
	}
	msg = append(msg, '\n')
	_, err := sio.conn.Write(msg)
	if err != nil {
		if sio.deej.Verbose() {
			logger.Warnw("Failed to write line to serial", "error", err, "line", util.BytesToString(msg))
		}
	}

	if sio.deej.Verbose() {
		logger.Debugw("Write new line", "line", util.BytesToString(msg), "cmd", sb.String())
	}

}
