var notification_windows = 0;
ti.Notification = function()
{
	var width = 300, height = 60, notificationDelay = 3000;
	var showing = false;
	var myid = 'notification_'+(notification_windows++);
	var mywindow = ti.Window.createWindow({
			width:width,
			height:height,
			transparency:.99,
			usingChrome:true,
			id:myid,
			visible:false,
			url:'app://blank'
	});
	var self = this;
	var title = '', message = '', icon = '';
	var hideTimer = null;
	mywindow.open();
	this.setTitle = function(value)
	{
		title = value;
	}
	this.setMessage = function(value)
	{
		message = value;
	}
	this.setIcon = function(value)
	{
		icon = value;
	}
	this.setDelay = function(value)
	{
		notificationDelay = value;
	}
	this.show = function(animate,autohide)
	{
		showing = true;
		if (hideTimer)
		{
			clearTimeout(hideTimer);
		}
		animate = (animate==null) ? true : animate;
		autohide = (autohide==null) ? true : autohide;
		mywindow.setX(screen.availWidth-width-20);
		mywindow.setY(10);
		mywindow.setTransparency(.99);
		mywindow.setURL('ti://notification/?title='+encodeURIComponent(title)+'&message='+encodeURIComponent(message)+'&icon='+encodeURIComponent(icon));
		mywindow.show(animate);
		if (autohide)
		{
			hideTimer = setTimeout(function()
			{
				self.hide();
			},notificationDelay + (animate ? 1000 : 0));
		}
	}
	this.hide = function(animate)
	{
		animate = (animate==null) ? true : animate;
		showing = false;
		if (hideTimer)
		{
			clearTimeout(hideTimer);
			hideTimer=null;
		}
		mywindow.hide(animate);
	}
	return this;
}
