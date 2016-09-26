### Spike-Triggered Average

**Requirements:** Spike Detector, Plot helper classes (included), Boost libraries
**Limitations:** None

![Spike-Triggered Average GUI](spike-triggered-average.png)

<!--start-->
<p><b>STA:</b></p><p> This plug-in computes an event-triggered average of the
input signal. The event trigger should provide a value of 1. The averaged
signal will update periodically. Click and drag on the plot to resize the
axes.</p>
<!--end-->

#### Input
1. input(0) - Input : Quantity to compute the spike-triggered average for
2. input(1) - Event Trigger : trigger that indicates the spike time/event (=1)

#### Output


#### Parameters
1. Trigger Threshold (V) - Threshold for detecting a trigger in volts
2. Interval (s) - Minimum time between events
3. Plot X-min (s) - Amount of time before the spike to include in average
4. Plot X-max (s) - Amount of time after the spike to include in average
5. Plot Y-min (V) - Minimum for y-axis on the plot
6. Plot Y-max (V) - Minimum for y-axis on the plot
7. Plot Refresh (ms) - Interval for updating plot

#### States
1. Event Count - Number of spikes/events included in the current average
2. Time (s) - Time (s)
