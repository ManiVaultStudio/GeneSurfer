// ManiVault invokes this function to set the plot data,
// when emitting qt_js_setDataInJS from the communication object
// The connection is established in qwebchannel.tools.js

var svg;

function drawChart(data, type) {
    // remove possible old chart 
    d3.select("div#container").select("*").remove();

    if (!data || data.length === 0) {
        log("GeneSurfer: bar_chart.tools.js: data empty")
        d3.select("div#container")
            .append("p")
            .text("No gene available above the threshold")
            .style("font-family", "Arial")
            .style("text-align", "center")
            .attr("class", "noDataText");
        return;
    }

    log("GeneSurfer: bar_chart.tools.js: draw chart");

    // --- size: match the widget (container) ---
    const container = document.querySelector("div#container");
    // If the widget provides a height, we use it; otherwise fall back to rows*20.
    var svgWidth = 400;
    var svgHeight = 900;
  
    var margin = { top: 30, right: 30, bottom: 30, left: 30 },
        width = svgWidth - margin.left - margin.right,
        height = svgHeight - margin.top - margin.bottom;

    // append svg
    svg = d3.select("div#container")
        .append("svg")
        .attr("width", svgWidth)
        .attr("height", svgHeight)
        // make it responsive to container resize if the container resizes:
        .style("width", "100%")
        .style("height", "100%")
        .append("g")
        .attr("transform", "translate(" + margin.left + "," + margin.top + ")");

    // optional filter
    var filterUnclustered = true;
    data = filterUnclustered ? data.filter(d => d.cluster !== "Unclustered") : data;

    // --- SCALES ---
    // y: genes (bands) down the page
    var y = d3.scaleBand()
        .range([height, 0])
        .domain(data.map(d => d.Gene))
        .padding(0.1);

    // x: numeric values left?right; pick domain from data
    var minVal = d3.min(data, d => d.Value);
    var maxVal = d3.max(data, d => d.Value);

    // For "Others": expect either [0, 1] when non-negative, or include negatives (min at least -1)
    // For "Moran": use a buffered domain around min/max (and always left?right)
    var x;
    if (type === "Moran") {
        var buffer = 0.05 * (maxVal - minVal);
        if (buffer === 0) buffer = 0.05;
        x = d3.scaleLinear()
            .domain([minVal - buffer, maxVal + buffer])
            .range([0, width])
            .nice();
    } else { // "Others" and everything else
        var domMin = (minVal >= 0) ? 0 : Math.min(-1, minVal);
        var domMax = Math.max(1, maxVal);
        x = d3.scaleLinear()
            .domain([domMin, domMax])
            .range([0, width])
            .nice();
    }

    // --- AXES ---
    // Bottom numeric axis
    svg.append("g")
        .attr("transform", "translate(0," + height + ")")
        .call(d3.axisBottom(x));

    // Left categorical axis (hide ticks/labels)
    //svg.append("g")
    //    .call(d3.axisLeft(y).tickFormat("").tickSize(0));

    // always draw axis line at x=0
    svg.append("line")
        .attr("x1", x(0))
        .attr("x2", x(0))
        .attr("y1", 0)
        .attr("y2", height)
        .attr("stroke", "black")
        .attr("stroke-width", 1);

    // --- ZERO LINE at x=0  ---
    if (x.domain()[0] < 0 && x.domain()[1] > 0) {
        svg.append("line")
            .attr("x1", x(0))
            .attr("x2", x(0))
            .attr("y1", 0)
            .attr("y2", height)
            .attr("stroke", "#888")
            .attr("stroke-width", 1);
    }

    // --- MORAN thresholds (vertical lines) ---
    if (type === "Moran") {
        const thresholds = [1.96, -1.96];
        const [d0, d1] = x.domain();
        thresholds.forEach(function (t) {
            if (t >= d0 && t <= d1) {
                svg.append("line")
                    .attr("x1", x(t))
                    .attr("x2", x(t))
                    .attr("y1", 0)
                    .attr("y2", height)
                    .attr("stroke", "red")
                    .attr("stroke-dasharray", "4")
                    .attr("stroke-width", 1);

                svg.append("text")
                    .attr("x", x(t) + 5)
                    .attr("y", 12)
                    .text(`Z = ${t.toFixed(2)}`)
                    .style("font-size", "12px")
                    .style("fill", "red")
                    .attr("text-anchor", "start")
                    .style("font-family", "Arial");
            } else {
                log("GeneSurfer: bar_chart.tools.js: threshold " + t + " is off-chart");
            }
        });
    }

    function colorToClass(color) {
        return color.replace('#', 'color_');
    }

    // tooltip (SVG text)
    var tooltip = svg.append("text")
        .attr("class", "tooltip")
        .style("font-size", "12px")
        .style("font-family", "Arial")
        .attr("alignment-baseline", "middle")
        .style("opacity", 0);

    // helpers for hover placement (use geometry so this matches highlightBars)
    function barEnds(d) {
        const x0 = x(0);
        const xv = x(d.Value);
        return { left: Math.min(x0, xv), right: Math.max(x0, xv) };
    }

    // hover handlers
    var mouseover = function (d) {
        var currentClass = colorToClass(d.categoryColor);
        d3.selectAll(".myRect").style("opacity", 0.2);
        d3.selectAll("." + currentClass).style("opacity", 1);
        d3.selectAll(".geneNameText").remove(); // clear highlight labels

        const ends = barEnds(d);
        const ymid = y(d.Gene) + y.bandwidth() / 2;
        const isPos = d.Value >= 0;

        tooltip
            .style("opacity", 1)
            .text(d.Gene)
            .attr("x", isPos ? (ends.right + 8) : (ends.left - 8))
            .attr("y", ymid)
            .attr("text-anchor", isPos ? "start" : "end")
            .attr("fill", "black");
    };

    var mouseleave = function () {
        d3.selectAll(".myRect").style("opacity", 0.8);
        tooltip.style("opacity", 0);
    };

    // --- BARS (from zero outwards) ---
    svg.selectAll("mybar")
        .data(data)
        .enter()
        .append("rect")
        .attr("y", d => y(d.Gene))
        .attr("x", d => Math.min(x(0), x(d.Value)))
        .attr("height", y.bandwidth())
        .attr("width", d => Math.abs(x(d.Value) - x(0)))
        .attr("fill", d => d.categoryColor)
        .attr("class", d => "myRect " + colorToClass(d.categoryColor))
        .on("mouseover", mouseover)
        .on("mouseleave", mouseleave)
        .on("click", function (d) {
            log("Clicked on bar: " + d.Gene + " with value: " + d.Value + " and cluster: " + d.cluster);
            d3.event.stopPropagation();
            passSelectionToQt(d.Gene);
        });

    // --- Legend ---
    //var clusters = Array.from(new Set(data.map(d => d.cluster)));
    //clusters.sort((a, b) => {
    //    if (a === "Unclustered") return 1;
    //    if (b === "Unclustered") return -1;
    //    var numA = parseInt(a.split(" ")[1]);
    //    var numB = parseInt(b.split(" ")[1]);
    //    return numA - numB;
    //});

    //var colorMap = {};
    //data.forEach(d => { colorMap[d.cluster] = d.categoryColor; });

    //var colorScale = d3.scaleOrdinal()
    //    .domain(clusters)
    //    .range(clusters.map(cluster => colorMap[cluster]));

    //var size = 20;

    //svg.selectAll("legendDots")
    //    .data(clusters)
    //    .enter()
    //    .append("rect")
    //    .attr("x", width - 300)
    //    .attr("y", (d, i) => i * (size + 5))
    //    .attr("width", size)
    //    .attr("height", size)
    //    .style("fill", d => colorScale(d));

    //svg.selectAll("legendLabels")
    //    .data(clusters)
    //    .enter()
    //    .append("text")
    //    .attr("x", width - 300 + size * 1.2)
    //    .attr("y", (d, i) => i * (size + 5) + (size / 2))
    //    .style("font-size", "12px")
    //    .style("font-family", "Arial")
    //    .style("fill", d => colorScale(d))
    //    .text(d => d)
    //    .attr("text-anchor", "left")
    //    .style("alignment-baseline", "middle");
}

// --- highlight selected bars: label BESIDE the bar (no rotation) ---
function highlightBars(dimensions) {
    if (!dimensions || dimensions.length === 0) {
        log("GeneSurfer: bar_chart.tools.js: dimensions empty");
        return;
    }

    log("GeneSurfer: bar_chart.tools.js: highlight bars");

    d3.selectAll(".myRect").style("opacity", 0.2);
    d3.selectAll(".geneNameText").remove();

    dimensions.forEach(function (dim) {
        d3.selectAll(".myRect").filter(function (d) {
            return d.Gene === dim;
        }).style("opacity", 1)
            .each(function (d) {
                var bar = d3.select(this);
                var x0 = parseFloat(bar.attr("x"));
                var w = parseFloat(bar.attr("width"));
                var left = x0;
                var right = x0 + w;

                var ypos = parseFloat(bar.attr("y")) + parseFloat(bar.attr("height")) / 2;
                var isPos = d.Value >= 0;
                var xpos = isPos ? (right + 25) : (left - 25);

                svg.append("text")
                    .attr("class", "geneNameText")
                    .attr("x", xpos)
                    .attr("y", ypos)
                    .attr("text-anchor", isPos ? "start" : "end")
                    .text(d.Gene)
                    .attr("fill", "black")
                    .attr("font-family", "Arial")
                    .attr("font-size", "12px")
                    .attr("alignment-baseline", "middle");
            });
    });
}
