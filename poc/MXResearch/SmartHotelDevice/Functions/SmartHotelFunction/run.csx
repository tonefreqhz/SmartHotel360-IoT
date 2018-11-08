#r "System.Runtime.Serialization"

using System;
using System.Collections.Generic;
using System.Configuration;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.WebSockets;
using System.Runtime.Serialization;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Azure.Devices;
using NAudio.Wave;
using Newtonsoft.Json;

public static async Task<HttpResponseMessage> Run(HttpRequestMessage req, TraceWriter log)
{
    byte[] data;
    string deviceName = "";
    try
    {
        data = await req.Content.ReadAsByteArrayAsync();
        string sourceValue = req.Headers.GetValues("source").FirstOrDefault();
    }
    catch(Exception ex)
    {
        return req.CreateResponse(HttpStatusCode.BadRequest, ex.Message);
    }

    var serviceClient = ServiceClient.CreateFromConnectionString(ConfigurationManager.AppSettings["iotHubConnectionString"]);

    await serviceClient.SendAsync(deviceName, new Message(Encoding.ASCII.GetBytes(final.Translation)));

    return req.CreateResponse(HttpStatusCode.OK, "OK");
}
