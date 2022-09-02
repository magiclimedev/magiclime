$(document).ready(function(){

    var logTable = $('#log_table');
    let sensorTable = $('#sensor_table');
    var markup;

    var ip = location.host;
    var socket = new WebSocket(`ws://${ip}:8080/ws`);

    // daemon for degrading 'active' status on dashboard rows
    let degradeStatusTime = 3600 * 3 * 1000;
    window.setInterval(function(){
        $('#sensor_table').children().each( function(){
            let lastSeen = $(this).attr('last-seen');
            let dateObj = Date.parse(lastSeen);
            let statusIcon = $(this).find('i[class*=fa-circle]');
            if (statusIcon.hasClass('active') && (Date.now() - dateObj) > degradeStatusTime)
            {
                console.log("More than 3600 seconds without update from sensor")
                statusIcon.removeClass('active');
                statusIcon.addClass('dead');               
            }
        });
    }, degradeStatusTime);

    socket.onopen = function(){
      console.log("Connected");
    };

    socket.onmessage = function (message) {
      console.log("Received: " + message.data);

      let obj;
      try{
          obj = JSON.parse(message.data);
      } catch (e){
          console.log("error: bad json in socket message: " + message.data);
          return;
      }

      switch (obj.channel){
        case "LOG_DATA":
          const url = new URL(window.location.href);
          
          let uid = url.pathname.replace("/sensors/", "");
          if (url.pathname === '/'){
            // console.log("On Dashboard page");
            let matchingRows = sensorTable.children(`[uid=${obj.uid}]`);
            matchingRows.addClass("flash")
            if (matchingRows.length > 0){
              matchingRows.attr('last-seen', obj.timestamp)
              matchingRows.children(':nth-child(4)').text(
                      obj.timestamp);
              let statusButton = matchingRows.find('i[class*=fa-circle]');
              statusButton.removeClass('alive');
              statusButton.removeClass('dead');
              statusButton.addClass('active');
            }
            else {    // if sensor is new
              let newRow = $('<tr></tr>');
              let newData = '<td>' + obj.uid + '</td>';
              newData += '<td>' + obj.uid + '</td>';
              newData += '<td>' + obj.sensor + '</td>';
              newData += '<td>' + obj.timestamp + '</td>';
              newData += '<td><i class="fa fa-circle active"></i></td>';
              newData += '<td><i class="fa fa-external-link-alt"></i>';
              newData += '</td>';
              newRow.append(newData);
              newRow.attr('uid', obj.uid);
              newRow.attr('onclick', "document.location=\"/sensors/" +
                  obj.uid + "\"");
              newRow.attr('last-seen', obj.timestamp)
              sensorTable.append(newRow);
            }
          }
          if (uid === obj.uid){
            logTable.prepend("<tr></tr>")
            let curRow = logTable.children(':first-child');
            curRow.addClass("flash")
            curRow.append("<td>" + obj.timestamp + "</td>");
            curRow.append("<td>" + obj.rss + "</td>");
            curRow.append("<td>" + obj.bat + "</td>");
            curRow.append("<td>" + obj.data + "</td>");

            if (logTable.children('tr').length > 10){
                logTable.children('tr').last().remove();
            }      
          }
          break;
        case "RECEIVER":
        if (!obj.status) {
            console.log("error: socket message with channel=RECEIVER missing " +
                "\"status\" attribute");
            return;
        }

        let recvStatus = $('#receiver-status')
        switch (obj.status){
        case "CON":
            recvStatus.text("Connected");
            recvStatus.css("color", "green");
            break;
        case "DIS":
            recvStatus.text("Disconnected");
            recvStatus.css("color", "red");
            break;
        default:
            console.log("error: unknown receiver status: " + obj.status);
        }
        break;
      }
    };

    socket.onclose = function(){
      console.log("Disconnected");
      markup = "<tr><td>Disconnected</td></tr>";
      logTable.append(markup);
    };

    var sendMessage = function(message) {
      console.log("Sending:" + message.data);
      socket.send(message.data);
    };

    // GUI Stuff

    // send a command to the serial port
    $("#cmd_send").click(function(ev){
      ev.preventDefault();
      var cmd = $('#cmd_value').val();
      sendMessage({ 'data' : cmd});
      $('#cmd_value').val("");
    });

    $('#clear').click(function(){
      logTable.empty();
    });

    let ncfSubmitted = false;

    $('#ncfNameInput').on('input', function(e){
        if (ncfSubmitted){
            // only show errors if form submit has already been attempted
            showNameFormErrors();
        }
    });

    $('#nameChangeForm').submit(function (e){
        e.preventDefault();
        e.stopPropagation();
        ncfSubmitted = true;    // indicate form submit has been attempted

        let form = e.target;
        if ($('#ncfNameInput')[0].validity.valid){
            // submit
            $.ajax({
                type:    "POST",
                url:     window.location.pathname,
                data:    $(form).serialize(),
                success: function(data) {
                    $('#sensorNameText').text(data.name);
                },
                error:   function(jqXHR, textStatus, err) {
                    console.log(`POST error: status = ${textStatus}, error: ${err}`);
                }
            });
            $('#nameFormModal').modal('toggle');    // close modal
        } else {
            showNameFormErrors()
        }

        return false;
    });

    function showNameFormErrors(){
        let nameInput = $('#ncfNameInput')[0]
        if (nameInput.validity.valid){
            $(nameInput).removeClass("is-invalid")
            $(nameInput).addClass("is-valid")
        } else {
            $(nameInput).removeClass("is-valid")
            $(nameInput).addClass("is-invalid")
            
            let errorMsg = ""
            if (nameInput.validity.valueMissing) {
                errorMsg = "Name is required!"
            } else if (nameInput.validity.tooLong) {
                errorMsg = "Name can be no longer than 32 characters."
            } else if (nameInput.validity.patternMismatch) {
                errorMsg = "Name must consist of letters and numbers only."
            } else {
                errorMsg = "Unknown input error (contact support@thingbits.com)."
            }
            $('#ncfNameError').text(errorMsg) 
        }
    }

    $('#nameFormModal').on('hidden.bs.modal', function() {
        ncfSubmitted = false;    // reset form-submitted status
        $('#ncfNameInput').removeClass("is-valid is-invalid");  // clear errors
        $(':input', '#nameFormModal').val('');    // clear text input box
    });

    /* ------ */

});

function submitNameForm(){
  let nfMod = $('#nameFormModal');
  $.ajax({
      type:    "POST",
      url:     window.location.pathname,
      data:    nfMod.find('form').serialize(),
      success: function(data) {
          $('#sensorNameText').text(data.name);
      },
      error:   function(jqXHR, textStatus, err) {
          console.log(`POST error: status = ${textStatus}, error: ${err}`);
      }
    });
  $('#nameFormModal').modal('toggle');    // close modal
}
