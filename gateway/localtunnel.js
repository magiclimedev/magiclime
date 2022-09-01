const localtunnel = require('localtunnel');

const TUNNEL_SUBDOMAIN = process.env.SUBDOMAIN || 'xxyyzz';
const TUNNEL_PORT = process.env.PORT || 8080;
const TUNNEL_HOST = process.env.TUNNEL || 'http://magiclime.io:1234';

let isTunnelConnected = false;
let isInitialized = false;

setInterval(() => {
  if (isTunnelConnected === true) {
    console.log('tunnel connection stable');
  }
  if (isTunnelConnected === false) {
    console.log('connecting now...');
    return createTunnel();
  }
  if (isInitialized === false) {
    console.log('initialized');
    return (isInitialized = true);
  }
}, 3000);

function createTunnel() {
  const tunnel = localtunnel(
    {
      port: TUNNEL_PORT,
      subdomain: TUNNEL_SUBDOMAIN,
      host: TUNNEL_HOST,
    },
    function (err, tunnel) {
      if (err) {
        console.log('localtunnel error:', err);
      } else {
        console.log('localtunnel connection on -->', tunnel.url);
        return (isTunnelConnected = true);
      }
    }
  );
  tunnel.on('close', function () {
    console.log('tunnel disconeected');
    return (isTunnelConnected = false);
  });
}
