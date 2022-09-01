const express = require('express');
const cors = require('cors');
require('dotenv').config();
var path = require('path');
var ip = require("ip");

const app = express();
// set the view engine to ejs
app.set('view engine', 'ejs');
app.set('views', path.join(__dirname, './gui/views'))

app.use(express.json()); //Used to parse JSON bodies
app.use(express.urlencoded({ extended: true })); //Parse URL-encoded bodies
app.use(cors());
app.use(express.static(path.join(__dirname, './gui')))

// Route includes
const guiRouter = require('./gui/routes/gui.router');
const apiRouter = require('./api/routes/api.router');

/* Routes */
app.use('/', guiRouter);
app.use('/api', apiRouter);

// Serve static files
app.use(express.static('web/gui'));

/** Listen * */
app.listen(80, () => {
   console.log(`Webserver started: http://${ip.address()}/`);
});

module.exports = app;
